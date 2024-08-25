// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/CallstackTracePrivate.h"

#if defined(PLATFORM_SUPPORTS_TRACE_WIN32_CALLSTACK) && UE_CALLSTACK_TRACE_ENABLED

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/MemoryBase.h"
#include "HAL/RunnableThread.h"
#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/CallstackTracePrivate.h"
#include "ProfilingDebugging/MemoryTrace.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#	include <winnt.h>
#	include <winternl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Misc/CoreDelegates.h"
#include "Trace/Trace.inl"
#include "HAL/Runnable.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Experimental/Containers/GrowOnlyLockFreeHash.h"

#include <atomic>

#ifndef UE_CALLSTACK_TRACE_FULL_CALLSTACKS
	#define UE_CALLSTACK_TRACE_FULL_CALLSTACKS 0
#endif

// 0=off, 1=stats, 2=validation, 3=truth_compare
#define BACKTRACE_DBGLVL 0

#define BACKTRACE_LOCK_FREE (1 && (BACKTRACE_DBGLVL == 0))

static bool GFullBacktraces = UE_CALLSTACK_TRACE_FULL_CALLSTACKS;
static bool GModulesAreInitialized = false;

// This implementation is using unwind tables which is results in very fast
// stack walking. In some cases this is not suitable, and we then fall back
// to the standard stack walking implementation.
#if !defined(UE_CALLSTACK_TRACE_USE_UNWIND_TABLES)
	#if defined(__clang__)
		#define UE_CALLSTACK_TRACE_USE_UNWIND_TABLES 0
	#else
		#define UE_CALLSTACK_TRACE_USE_UNWIND_TABLES 1
	#endif
#endif

#if UE_CALLSTACK_TRACE_USE_UNWIND_TABLES

/*
 * Windows' x64 binaries contain a ".pdata" section that describes the location
 * and size of its functions and details on how to unwind them. The unwind
 * information includes descriptions about a function's stack frame size and
 * the non-volatile registers it pushes onto the stack. From this we can
 * calculate where a call instruction wrote its return address. This is enough
 * to walk the callstack and by caching this information it can be done
 * efficiently.
 *
 * Some functions need a variable amount of stack (such as those that use
 * alloc() for example) will use a frame pointer. Frame pointers involve saving
 * and restoring the stack pointer in the function's prologue/epilogue. This
 * frees the function up to modify the stack pointer arbitrarily. This
 * significantly complicates establishing where a return address is, so this
 * pdata scheme of walking the stack just doesn't support functions like this.
 * Walking stops if it encounters such a function. Fortunately there are
 * usually very few such functions, saving us from having to read and track
 * non-volatile registers which adds a significant amount of work.
 *
 * A further optimisation is to to assume we are only interested methods that
 * are part of engine or game code. As such we only build lookup tables for
 * such modules and never accept OS or third party modules. Backtracing stops
 * if an address is encountered which doesn't map to a known module.
 */

////////////////////////////////////////////////////////////////////////////////
static uint32 AddressToId(UPTRINT Address)
{
	return uint32(Address >> 16);
}

static UPTRINT IdToAddress(uint32 Id)
{
	return static_cast<uint32>(UPTRINT(Id) << 16);
}

struct FIdPredicate
{
	template <class T> bool operator () (uint32 Id, const T& Item) const { return Id < Item.Id; }
	template <class T> bool operator () (const T& Item, uint32 Id) const { return Item.Id < Id; }
};

////////////////////////////////////////////////////////////////////////////////
struct FUnwindInfo
{
	uint8	Version : 3;
	uint8	Flags : 5;
	uint8	PrologBytes;
	uint8	NumUnwindCodes;
	uint8	FrameReg	: 4;
	uint8	FrameRspBias : 4;
};

struct FUnwindCode
{
	uint8	PrologOffset;
	uint8	OpCode : 4;
	uint8	OpInfo : 4;
	uint16	Params[];
};

enum
{
	UWOP_PUSH_NONVOL		= 0,	// 1 node
	UWOP_ALLOC_LARGE		= 1,	// 2 or 3 nodes
	UWOP_ALLOC_SMALL		= 2,	// 1 node
	UWOP_SET_FPREG			= 3,	// 1 node
	UWOP_SAVE_NONVOL		= 4,	// 2 nodes
	UWOP_SAVE_NONVOL_FAR	= 5,	// 3 nodes
	UWOP_SAVE_XMM128		= 8,	// 2 nodes
	UWOP_SAVE_XMM128_FAR	= 9,	// 3 nodes
	UWOP_PUSH_MACHFRAME		= 10,	// 1 node
};

////////////////////////////////////////////////////////////////////////////////
class FBacktracer
{
public:
							FBacktracer(FMalloc* InMalloc);
							~FBacktracer();
	static FBacktracer*		Get();
	void					AddModule(UPTRINT Base, const TCHAR* Name);
	void					RemoveModule(UPTRINT Base);
	uint32					GetBacktraceId(void* AddressOfReturnAddress);

private:
	struct FFunction
	{
		uint32				Id;
		int32				RspBias;
#if BACKTRACE_DBGLVL >= 2
		uint32				Size;
		const FUnwindInfo*	UnwindInfo;
#endif
	};

	struct FModule
	{
		uint32				Id;
		uint32				IdSize;
		uint32				NumFunctions;
#if BACKTRACE_DBGLVL >= 1
		uint16				NumFpTypes;
		//uint16			*padding*
#else
		//uint32			*padding*
#endif
		FFunction*			Functions;
	};

	struct FLookupState
	{
		FModule				Module;
	};

	struct FFunctionLookupSetEntry
	{
		// Bottom 48 bits are key (pointer), top 16 bits are data (RSP bias for function)
		std::atomic_uint64_t Data;

		inline uint64 GetKey() const { return Data.load(std::memory_order_relaxed) & 0xffffffffffffull; }
		inline int32 GetValue() const { return static_cast<int64>(Data.load(std::memory_order_relaxed)) >> 48; }
		inline bool IsEmpty() const { return Data.load(std::memory_order_relaxed) == 0; }
		inline void SetKeyValue(uint64 Key, int32 Value)
		{
			Data.store(Key | (static_cast<int64>(Value) << 48), std::memory_order_relaxed);
		}
		static inline uint32 KeyHash(uint64 Key)
		{
			// 64 bit pointer to 32 bit hash
			Key = (~Key) + (Key << 21);
			Key = Key ^ (Key >> 24);
			Key = Key * 265;
			Key = Key ^ (Key >> 14);
			Key = Key * 21;
			Key = Key ^ (Key >> 28);
			Key = Key + (Key << 31);
			return static_cast<uint32>(Key);
		}
		static void ClearEntries(FFunctionLookupSetEntry* Entries, int32 EntryCount)
		{
			memset(Entries, 0, EntryCount * sizeof(FFunctionLookupSetEntry));
		}
	};
	typedef TGrowOnlyLockFreeHash<FFunctionLookupSetEntry, uint64, int32> FFunctionLookupSet;

	const FFunction*		LookupFunction(UPTRINT Address, FLookupState& State) const;
	static FBacktracer*		Instance;
	mutable FCriticalSection Lock;
	FModule*				Modules;
	int32					ModulesNum;
	int32					ModulesCapacity;
	FMalloc*				Malloc;
	FCallstackTracer		CallstackTracer;
#if BACKTRACE_LOCK_FREE
	mutable FFunctionLookupSet	FunctionLookups;
	mutable bool				bReentranceCheck = false;
#endif
#if BACKTRACE_DBGLVL >= 1
	mutable uint32			NumFpTruncations = 0;
	mutable uint32			TotalFunctions = 0;
#endif
};

////////////////////////////////////////////////////////////////////////////////
FBacktracer* FBacktracer::Instance = nullptr;

////////////////////////////////////////////////////////////////////////////////
FBacktracer::FBacktracer(FMalloc* InMalloc)
	: Malloc(InMalloc)
	, CallstackTracer(InMalloc)
#if BACKTRACE_LOCK_FREE
	, FunctionLookups(InMalloc)
#endif
{
#if BACKTRACE_LOCK_FREE
	FunctionLookups.Reserve(512 * 1024);		// 4 MB
#endif
	ModulesCapacity = 8;
	ModulesNum = 0;
	Modules = (FModule*)Malloc->Malloc(sizeof(FModule) * ModulesCapacity);

	Instance = this;
}

////////////////////////////////////////////////////////////////////////////////
FBacktracer::~FBacktracer()
{
	TArrayView<FModule> ModulesView(Modules, ModulesNum);
	for (FModule& Module : ModulesView)
	{
		Malloc->Free(Module.Functions);
	}
}

////////////////////////////////////////////////////////////////////////////////
FBacktracer* FBacktracer::Get()
{
	return Instance;
}

////////////////////////////////////////////////////////////////////////////////
void FBacktracer::AddModule(UPTRINT ModuleBase, const TCHAR* Name)
{
	const auto* DosHeader = (IMAGE_DOS_HEADER*)ModuleBase;
	const auto* NtHeader = (IMAGE_NT_HEADERS*)(ModuleBase + DosHeader->e_lfanew);
	const IMAGE_FILE_HEADER* FileHeader = &(NtHeader->FileHeader);

	if(!GFullBacktraces)
	{
		int32 NameLen = FCString::Strlen(Name);
		if (!(NameLen > 4 && FCString::Strcmp(Name + NameLen - 4, TEXT(".exe")) == 0) && // we want always load an .exe file
			(FCString::Strfind(Name, TEXT("Binaries")) == nullptr || FCString::Strfind(Name, TEXT("ThirdParty")) != nullptr))
		{
			return;
		}
	}

	uint32 NumSections = FileHeader->NumberOfSections;
	const auto* Sections = (IMAGE_SECTION_HEADER*)(UPTRINT(&(NtHeader->OptionalHeader)) + FileHeader->SizeOfOptionalHeader);

	// Find ".pdata" section
	UPTRINT PdataBase = 0;
	UPTRINT PdataEnd = 0;
	for (uint32 i = 0; i < NumSections; ++i)
	{
		const IMAGE_SECTION_HEADER* Section = Sections + i;
		if (*(uint64*)(Section->Name) == 0x61'74'61'64'70'2eull) // Sections names are eight bytes and zero padded. This constant is '.pdata'
		{
			PdataBase = ModuleBase + Section->VirtualAddress;
			PdataEnd = PdataBase + Section->SizeOfRawData;
			break;
		}
	}

	if (PdataBase == 0)
	{
		return;
	}

	// Count the number of functions. The assumption here is that if we have got this far then there is at least one function
	uint32 NumFunctions = uint32(PdataEnd - PdataBase) / sizeof(RUNTIME_FUNCTION);
	if (NumFunctions == 0)
	{
		return;
	}

	const auto* FunctionTables = (RUNTIME_FUNCTION*)PdataBase;
	do
	{
		const RUNTIME_FUNCTION* Function = FunctionTables + NumFunctions - 1;
		if (uint32(Function->BeginAddress) < uint32(Function->EndAddress))
		{
			break;
		}

		--NumFunctions;
	}
	while (NumFunctions != 0);
		
	// Allocate some space for the module's function-to-frame-size table
	auto* OutTable = (FFunction*)Malloc->Malloc(sizeof(FFunction) * NumFunctions);
	FFunction* OutTableCursor = OutTable;

	// Extract frame size for each function from pdata's unwind codes.
	uint32 NumFpFuncs = 0;
	for (uint32 i = 0; i < NumFunctions; ++i)
	{
		const RUNTIME_FUNCTION* FunctionTable = FunctionTables + i;

		UPTRINT UnwindInfoAddr = ModuleBase + FunctionTable->UnwindInfoAddress;
		const auto* UnwindInfo = (FUnwindInfo*)UnwindInfoAddr;

		if (UnwindInfo->Version != 1)
		{
			/* some v2s have been seen in msvc. Always seem to be assembly
			 * routines (memset, memcpy, etc) */
			continue;
		}

		int32 FpInfo = 0;
		int32 RspBias = 0;

#if BACKTRACE_DBGLVL >= 2
		uint32 PrologVerify = UnwindInfo->PrologBytes;
#endif

		const auto* Code = (FUnwindCode*)(UnwindInfo + 1);
		const auto* EndCode = Code + UnwindInfo->NumUnwindCodes;
		while (Code < EndCode)
		{
#if BACKTRACE_DBGLVL >= 2
			if (Code->PrologOffset > PrologVerify)
			{
				PLATFORM_BREAK();
			}
			PrologVerify = Code->PrologOffset;
#endif

			switch (Code->OpCode)
			{
			case UWOP_PUSH_NONVOL:
				RspBias += 8;
				Code += 1;
				break;

			case UWOP_ALLOC_LARGE:
				if (Code->OpInfo)
				{
					RspBias += *(uint32*)(Code->Params);
					Code += 3;
				}
				else
				{
					RspBias += Code->Params[0] * 8;
					Code += 2;
				}
				break;

			case UWOP_ALLOC_SMALL:
				RspBias += (Code->OpInfo * 8) + 8;
				Code += 1;
				break;

			case UWOP_SET_FPREG:
				// Function will adjust RSP (e.g. through use of alloca()) so it
				// uses a frame pointer register. There's instructions like;
				//
				//   push FRAME_REG
				//   lea FRAME_REG, [rsp + (FRAME_RSP_BIAS * 16)]
				//   ...
				//   add rsp, rax
				//   ...
				//   sub rsp, FRAME_RSP_BIAS * 16
				//   pop FRAME_REG
				//   ret
				//
				// To recover the stack frame we would need to track non-volatile
				// registers which adds a lot of overhead for a small subset of
				// functions. Instead we'll end backtraces at these functions.


				// MSB is set to detect variable sized frames that we can't proceed
				// past when back-tracing.
				NumFpFuncs++;
				FpInfo |= 0x80000000 | (uint32(UnwindInfo->FrameReg) << 27) | (uint32(UnwindInfo->FrameRspBias) << 23);
				Code += 1;
				break;

			case UWOP_PUSH_MACHFRAME:
				RspBias = Code->OpInfo ? 48 : 40;
				Code += 1;
				break;

			case UWOP_SAVE_NONVOL:		Code += 2; break; /* saves are movs instead of pushes */
			case UWOP_SAVE_NONVOL_FAR:	Code += 3; break;
			case UWOP_SAVE_XMM128:		Code += 2; break;
			case UWOP_SAVE_XMM128_FAR:	Code += 3; break;

			default:
#if BACKTRACE_DBGLVL >= 2
				PLATFORM_BREAK();
#endif
				break;
			}
		}

		// "Chained" simply means that multiple RUNTIME_FUNCTIONs pertains to a
		// single actual function in the .text segment.
		bool bIsChained = (UnwindInfo->Flags & UNW_FLAG_CHAININFO);

		RspBias /= sizeof(void*);	// stack push/popds in units of one machine word
		RspBias += !bIsChained;		// and one extra push for the ret address
		RspBias |= FpInfo;			// pack in details about possible frame pointer

		if (bIsChained)
		{
			OutTableCursor[-1].RspBias += RspBias;
#if BACKTRACE_DBGLVL >= 2
			OutTableCursor[-1].Size += (FunctionTable->EndAddress - FunctionTable->BeginAddress);
#endif
		}
		else
		{
			*OutTableCursor = {
				FunctionTable->BeginAddress,
				RspBias,
#if BACKTRACE_DBGLVL >= 2
				FunctionTable->EndAddress - FunctionTable->BeginAddress,
				UnwindInfo,
#endif
			};

			++OutTableCursor;
		}
	}

	UPTRINT ModuleSize = NtHeader->OptionalHeader.SizeOfImage;
	ModuleSize += 0xffff; // to align up to next 64K page. it'll get shifted by AddressToId()

	FModule Module = {
		AddressToId(ModuleBase),
		AddressToId(ModuleSize),
		uint32(UPTRINT(OutTableCursor - OutTable)),
#if BACKTRACE_DBGLVL >= 1
		uint16(NumFpFuncs),
#endif
		OutTable,
	};

	{
		FScopeLock _(&Lock);

		TArrayView<FModule> ModulesView(Modules, ModulesNum);
		int32 Index = Algo::UpperBound(ModulesView, Module.Id, FIdPredicate());

		if (ModulesNum + 1 > ModulesCapacity)
		{
			ModulesCapacity += 8;
			Modules = (FModule*) Malloc->Realloc(Modules, sizeof(FModule)* ModulesCapacity);
		}
		Modules[ModulesNum++] = Module;
		Algo::Sort(TArrayView<FModule>(Modules, ModulesNum), [](const FModule& A, const FModule& B) { return A.Id < B.Id; });
	}

#if BACKTRACE_DBGLVL >= 1
	NumFpTruncations += NumFpFuncs;
	TotalFunctions += NumFunctions;
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FBacktracer::RemoveModule(UPTRINT ModuleBase)
{
	// When Windows' RequestExit() is called it hard-terminates all threads except
	// the main thread and then proceeds to unload the process' DLLs. This hard 
	// thread termination can result is dangling locked locks. Not an issue as
	// the rule is "do not do anything multithreaded in DLL load/unload". And here
	// we are, taking write locks during DLL unload which is, quite unsurprisingly,
	// deadlocking. In reality tracking Windows' DLL unloads doesn't tell us
	// anything due to how DLLs and processes' address spaces work. So we will...
#if defined PLATFORM_WINDOWS
	return;
#else

	FScopeLock _(&Lock);

	uint32 ModuleId = AddressToId(ModuleBase);
	TArrayView<FModule> ModulesView(Modules, ModulesNum);
	int32 Index = Algo::LowerBound(ModulesView, ModuleId, FIdPredicate());
	if (Index >= ModulesNum)
	{
		return;
	}

	const FModule& Module = Modules[Index];
	if (Module.Id != ModuleId)
	{
		return;
	}

#if BACKTRACE_DBGLVL >= 1
	NumFpTruncations -= Module.NumFpTypes;
	TotalFunctions -= Module.NumFunctions;
#endif

	// no code should be executing at this point so we can safely free the
	// table knowing know one is looking at it.
	Malloc->Free(Module.Functions);

	for (SIZE_T i = Index; i < ModulesNum; i++)
	{
		Modules[i] = Modules[i + 1];
	}

	--ModulesNum;
#endif
}

////////////////////////////////////////////////////////////////////////////////
const FBacktracer::FFunction* FBacktracer::LookupFunction(UPTRINT Address, FLookupState& State) const
{
	// This function caches the previous module look up. The theory here is that
	// a series of return address in a backtrace often cluster around one module

	FIdPredicate IdPredicate;

	// Look up the module that Address belongs to.
	uint32 AddressId = AddressToId(Address);
	if ((AddressId - State.Module.Id) >= State.Module.IdSize)
	{
		TArrayView<FModule> ModulesView(Modules, ModulesNum);
		uint32 Index = Algo::UpperBound(ModulesView, AddressId, IdPredicate);
		if (Index == 0)
		{
			return nullptr;
		}

		State.Module = Modules[Index - 1];
	}

	// Check that the address is within the address space of the best-found module
	const FModule* Module = &(State.Module);
	if ((AddressId - Module->Id) >= Module->IdSize)
	{
		return nullptr;
	}

	// Now we've a module we have a table of functions and their stack sizes so
	// we can get the frame size for Address
	uint32 FuncId = uint32(Address - IdToAddress(Module->Id)); 
	TArrayView<FFunction> FuncsView(Module->Functions, Module->NumFunctions);
	uint32 Index = Algo::UpperBound(FuncsView, FuncId, IdPredicate);
	if (Index == 0)
	{
		return nullptr;
	}

	const FFunction* Function = Module->Functions + (Index - 1);
#if BACKTRACE_DBGLVL >= 2
	if ((FuncId - Function->Id) >= Function->Size)
	{
		PLATFORM_BREAK();
		return nullptr;
	}
#endif
	return Function;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FBacktracer::GetBacktraceId(void* AddressOfReturnAddress) 
{
	FLookupState LookupState = {};
	uint64 Frames[256];

	UPTRINT* StackPointer = (UPTRINT*)AddressOfReturnAddress;

#if BACKTRACE_DBGLVL >= 3
	UPTRINT TruthBacktrace[1024];
	uint32 NumTruth = RtlCaptureStackBackTrace(0, 1024, (void**)TruthBacktrace, nullptr);
	UPTRINT* TruthCursor = TruthBacktrace;
	for (; *TruthCursor != *StackPointer; ++TruthCursor);
#endif

#if BACKTRACE_DBGLVL >= 2
	struct { void* Sp; void* Ip; const FFunction* Function; } Backtrace[1024] = {};
	uint32 NumBacktrace = 0;
#endif

	uint64 BacktraceHash = 0;
	uint32 FrameIdx = 0;

#if BACKTRACE_LOCK_FREE
	// When running lock free, we defer the lock until a lock free function lookup fails
	bool Locked = false;
#else
	FScopeLock _(&Lock);
#endif
	do
	{
		UPTRINT RetAddr = *StackPointer;
		
		Frames[FrameIdx++] = RetAddr;

		// This is a simple order-dependent LCG. Should be sufficient enough
		BacktraceHash += RetAddr;
		BacktraceHash *= 0x30be8efa499c249dull;

#if BACKTRACE_LOCK_FREE
		int32 RspBias;
		bool bIsAlreadyInTable;
		FunctionLookups.Find(RetAddr, &RspBias, &bIsAlreadyInTable);
		if (bIsAlreadyInTable)
		{
			if (RspBias < 0)
			{
				break;
			}
			else
			{
				StackPointer += RspBias;
				continue;
			}
		}
		if (!Locked)
		{
			Lock.Lock();
			Locked = true;

			// If FunctionLookups.Emplace triggers a reallocation, it can cause an infinite recursion
			// when the allocation reenters the stack trace code.  We need to break out of the recursion
			// in that case, and let the allocation complete, with the assumption that we don't care
			// about call stacks for internal allocations in the memory reporting system.  The "Lock()"
			// above will only fall through with this flag set if it's a second lock in the same thread.
			if (bReentranceCheck)
			{
				break;
			}
		}
#endif  // BACKTRACE_LOCK_FREE

		const FFunction* Function = LookupFunction(RetAddr, LookupState);
		if (Function == nullptr)
		{
#if BACKTRACE_LOCK_FREE
			// LookupFunction fails when modules are not yet registered. In this case, we do not want the address
			// to be added to the lookup map, but to retry the lookup later when modules are properly registered.
			if (GModulesAreInitialized)
			{
				TGuardValue<bool> ReentranceGuard(bReentranceCheck, true);
				FunctionLookups.Emplace(RetAddr, -1);
			}
#endif
			break;
		}

#if BACKTRACE_LOCK_FREE
		{
			// This conversion improves probing performance for the hash set. Additionally it is critical 
			// to avoid incorrect values when RspBias is compressed into 16 bits in the hash map.
			int32 StoreBias = Function->RspBias < 0 ? -1 : Function->RspBias;
			TGuardValue<bool> ReentranceGuard(bReentranceCheck, true);
			FunctionLookups.Emplace(RetAddr, StoreBias);
		}
#endif

#if BACKTRACE_DBGLVL >= 2
		if (NumBacktrace < 1024)
		{
			Backtrace[NumBacktrace++] = {
				StackPointer,
				(void*)RetAddr,
				Function,
			};
		}
#endif

		if (Function->RspBias < 0)
		{
			// This is a frame with a variable-sized stack pointer. We don't
			// track enough information to proceed.
#if BACKTRACE_DBGLVL >= 1
			NumFpTruncations++;
#endif
			break;
		}

		StackPointer += Function->RspBias;
	}
	// Trunkate callstacks longer than MaxStackDepth
	while (*StackPointer && FrameIdx < UE_ARRAY_COUNT(Frames));

	// Build the backtrace entry for submission
	FCallstackTracer::FBacktraceEntry BacktraceEntry;
	BacktraceEntry.Hash = BacktraceHash;
	BacktraceEntry.FrameCount = FrameIdx;
	BacktraceEntry.Frames = Frames;

#if BACKTRACE_DBGLVL >= 3
	for (uint32 i = 0; i < NumBacktrace; ++i)
	{
		if ((void*)TruthCursor[i] != Backtrace[i].Ip)
		{
			PLATFORM_BREAK();
			break;
		}
	}
#endif

#if BACKTRACE_LOCK_FREE
	if (Locked)
	{
		Lock.Unlock();
	}
#endif
	// Add to queue to be processed. This might block until there is room in the
	// queue (i.e. the processing thread has caught up processing).
	return CallstackTracer.AddCallstack(BacktraceEntry);
}

#else // UE_CALLSTACK_TRACE_USE_UNWIND_TABLES

////////////////////////////////////////////////////////////////////////////////
class FBacktracer
{
public:
	FBacktracer(FMalloc* InMalloc);
	~FBacktracer();
	static FBacktracer*	Get();
	uint32 GetBacktraceId(void* AddressOfReturnAddress);
	void AddModule(UPTRINT Base, const TCHAR* Name) {}
	void RemoveModule(UPTRINT Base) {}

private:
	static FBacktracer* Instance;
	FMalloc* Malloc;
	FCallstackTracer CallstackTracer;
};

////////////////////////////////////////////////////////////////////////////////
FBacktracer* FBacktracer::Instance = nullptr;

////////////////////////////////////////////////////////////////////////////////
FBacktracer::FBacktracer(FMalloc* InMalloc)
	: Malloc(InMalloc)
	, CallstackTracer(InMalloc)
{
	Instance = this;
}

////////////////////////////////////////////////////////////////////////////////
FBacktracer::~FBacktracer()
{
}

////////////////////////////////////////////////////////////////////////////////
FBacktracer* FBacktracer::Get()
{
	return Instance;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FBacktracer::GetBacktraceId(void* AddressOfReturnAddress) 
{
#if !UE_BUILD_SHIPPING
	const uint64 ReturnAddress = *(uint64*)AddressOfReturnAddress;
	uint64 StackFrames[256];
	int32 NumStackFrames = FPlatformStackWalk::CaptureStackBackTrace(StackFrames, UE_ARRAY_COUNT(StackFrames));
	if (NumStackFrames > 0)
	{
		FCallstackTracer::FBacktraceEntry BacktraceEntry;
		uint64 BacktraceId = 0;
		uint32 FrameIdx = 0;
		bool bUseAddress = false;
		for (int32 Index = 0; Index < NumStackFrames; Index++)
		{
			if (!bUseAddress)
			{
				// start using backtrace only after ReturnAddress
				if (StackFrames[Index] == (uint64)ReturnAddress)
				{
					bUseAddress = true;
				}
			}
			if (bUseAddress || NumStackFrames == 1)
			{
				uint64 RetAddr = StackFrames[Index];
				StackFrames[FrameIdx++] = RetAddr;

				// This is a simple order-dependent LCG. Should be sufficient enough
				BacktraceId += RetAddr;
				BacktraceId *= 0x30be8efa499c249dull;
			}
		}

		// Save the collected id
		BacktraceEntry.Hash = BacktraceId;
		BacktraceEntry.FrameCount = FrameIdx;
		BacktraceEntry.Frames = StackFrames;

		// Add to queue to be processed. This might block until there is room in the
		// queue (i.e. the processing thread has caught up processing).
		return CallstackTracer.AddCallstack(BacktraceEntry);
	}
#endif

	return 0;
}
#endif // UE_CALLSTACK_TRACE_USE_UNWIND_TABLES

////////////////////////////////////////////////////////////////////////////////
void Modules_Create(FMalloc*);
void Modules_Subscribe(void (*)(bool, void*, const TCHAR*));
void Modules_Initialize();

////////////////////////////////////////////////////////////////////////////////
void CallstackTrace_CreateInternal(FMalloc* Malloc)
{
	if (FBacktracer::Get() != nullptr)
	{
		return;
	}

	// Allocate, construct and intentionally leak backtracer
	void* Alloc = Malloc->Malloc(sizeof(FBacktracer), alignof(FBacktracer));
	new (Alloc) FBacktracer(Malloc);

	const TCHAR* CmdLine = ::GetCommandLineW();
	if (const TCHAR* TraceArg = FCString::Stristr(CmdLine, TEXT("-tracefullcallstacks")))
	{
		GFullBacktraces = true;
	}

	Modules_Create(Malloc);
	Modules_Subscribe(
		[] (bool bLoad, void* Module, const TCHAR* Name)
		{
			bLoad
				? FBacktracer::Get()->AddModule(UPTRINT(Module), Name) //-V522
				: FBacktracer::Get()->RemoveModule(UPTRINT(Module));
		}
	);
}

////////////////////////////////////////////////////////////////////////////////
void CallstackTrace_InitializeInternal()
{
	Modules_Initialize();
	GModulesAreInitialized = true;
}

////////////////////////////////////////////////////////////////////////////////
uint32 CallstackTrace_GetCurrentId()
{
	void* AddressOfReturnAddress = PLATFORM_RETURN_ADDRESS_FOR_CALLSTACKTRACING();
	if (FBacktracer* Instance = FBacktracer::Get())
	{
		return Instance->GetBacktraceId(AddressOfReturnAddress);
	}

	return 0;
}

#endif // defined(PLATFORM_SUPPORTS_TRACE_WIN32_CALLSTACK) && UE_CALLSTACK_TRACE_ENABLED
