// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/MemoryTrace.h"

#include "ProfilingDebugging/CallstackTrace.h"
#include "ProfilingDebugging/TraceMalloc.h"

#if UE_MEMORY_TRACE_ENABLED

#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "HAL/MemoryBase.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CString.h"
#include "Trace/Trace.inl"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <shellapi.h>
#include <winternl.h>
#include <intrin.h>

#if (NTDDI_VERSION >= NTDDI_WIN10_RS4)
#pragma comment(lib, "mincore.lib") // VirtualAlloc2
#endif

////////////////////////////////////////////////////////////////////////////////
FMalloc* MemoryTrace_CreateInternal(FMalloc*, int32, const WIDECHAR* const*);

////////////////////////////////////////////////////////////////////////////////
struct FAddrPack
{
			FAddrPack() = default;
			FAddrPack(UPTRINT Addr, uint16 Value) { Set(Addr, Value); }
	void	Set(UPTRINT Addr, uint16 Value) { Inner = uint64(Addr) | (uint64(Value) << 48ull); }
	uint64	Inner;
};
static_assert(sizeof(FAddrPack) == sizeof(uint64), "");

////////////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_SUPPORTS_TRACE_WIN32_VIRTUAL_MEMORY_HOOKS)
class FTextSectionEditor
{
public:
							~FTextSectionEditor();
	template <typename T> T*Hook(T* Target, T* HookFunction);

private:
	struct FTrampolineBlock
	{
		FTrampolineBlock*	Next;
		uint32				Size;
		uint32				Used;
	};

	static void*			GetActualAddress(void* Function);
	FTrampolineBlock*		AllocateTrampolineBlock(void* Reference);
	uint8*					AllocateTrampoline(void* Reference, unsigned int Size);
	void*					HookImpl(void* Target, void* HookFunction);
	FTrampolineBlock*		HeadBlock = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
FTextSectionEditor::~FTextSectionEditor()
{
	for (FTrampolineBlock* Block = HeadBlock; Block != nullptr; Block = Block->Next)
	{
		DWORD Unused;
		VirtualProtect(Block, Block->Size, PAGE_EXECUTE_READ, &Unused);
	}

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
}

////////////////////////////////////////////////////////////////////////////////
void* FTextSectionEditor::GetActualAddress(void* Function)
{
	// Follow a jmp instruction (0xff/4 only for now) at function and returns
	// where it would jmp to.

	uint8* Addr = (uint8*)Function;
	int Offset = unsigned(Addr[0] & 0xf0) == 0x40; // REX prefix
	if (Addr[Offset + 0] == 0xff && Addr[Offset + 1] == 0x25)
	{
		Addr += Offset;
		Addr = *(uint8**)(Addr + 6 + *(uint32*)(Addr + 2));
	}
	return Addr;
}

////////////////////////////////////////////////////////////////////////////////
FTextSectionEditor::FTrampolineBlock* FTextSectionEditor::AllocateTrampolineBlock(void* Reference)
{
	static const SIZE_T BlockSize = 0x10000; // 64KB is Windows' canonical granularity

	// Find the start of the main allocation that mapped Reference
	MEMORY_BASIC_INFORMATION MemInfo;
	VirtualQuery(Reference, &MemInfo, sizeof(MemInfo));
	auto* Ptr = (uint8*)(MemInfo.AllocationBase);

	// Step backwards one block at a time and try and allocate that address
	while (true)
	{
		Ptr -= BlockSize;
		if (VirtualAlloc(Ptr, BlockSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) != nullptr)
		{
			break;
		}

		UPTRINT Distance = UPTRINT(Reference) - UPTRINT(Ptr);
		if (Distance >= 1ull << 31)
		{
			check(!"Failed to allocate trampoline blocks for memory tracing hooks");
		}
	}

	auto* Block = (FTrampolineBlock*)Ptr;
	Block->Next = HeadBlock;
	Block->Size = BlockSize;
	Block->Used = sizeof(FTrampolineBlock);
	HeadBlock = Block;

	return Block;
}

////////////////////////////////////////////////////////////////////////////////
uint8* FTextSectionEditor::AllocateTrampoline(void* Reference, unsigned int Size)
{
	// Try and find a block that's within 2^31 bytes before Reference
	FTrampolineBlock* Block;
	for (Block = HeadBlock; Block != nullptr; Block = Block->Next)
	{
		UPTRINT Distance = UPTRINT(Reference) - UPTRINT(Block);
		if (Distance < 1ull << 31)
		{
			break;
		}
	}

	// If we didn't find a block then we need to allocate a new one.
	if (Block == nullptr)
	{
		Block = AllocateTrampolineBlock(Reference);
	}

	// Allocate space for the trampoline.
	uint32 NextUsed = Block->Used + Size;
	if (NextUsed > Block->Size)
	{
		// Block is full. We could allocate a new block here but as it is not
		// expected that so many hooks will be made this path shouldn't happen
		check(!"Unable to allocate memory for memory tracing's hooks");
	}

	uint8* Out = (uint8*)Block + Block->Used;
	Block->Used = NextUsed;

	return Out;
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
T* FTextSectionEditor::Hook(T* Target, T* HookFunction)
{
	return (T*)HookImpl((void*)Target, (void*)HookFunction);
}

////////////////////////////////////////////////////////////////////////////////
void* FTextSectionEditor::HookImpl(void* Target, void* HookFunction)
{
	Target = GetActualAddress(Target);

	// Very rudimentary x86_64 instruction length decoding that only supports op
	// code ranges (0x80,0x8b) and (0x50,0x5f). Enough for simple prologues
	uint8* __restrict Start = (uint8*)Target;
	const uint8* Read = Start;
	do
	{
		Read += (Read[0] & 0xf0) == 0x40; // REX prefix
		uint8 Inst = *Read++;
		if (unsigned(Inst - 0x80) < 0x0cu)
		{
			uint8 ModRm = *Read++;
			Read += ((ModRm & 0300) < 0300) & ((ModRm & 0007) == 0004); // SIB
			switch (ModRm & 0300) // Disp[8|32]
			{
			case 0100: Read += 1; break;
			case 0200: Read += 5; break;
			}
			Read += (Inst == 0x83);
		}
		else if (unsigned(Inst - 0x50) >= 0x10u)
			check(!"Unknown instruction");
	}
	while (Read - Start < 6);

	static const int TrampolineSize = 24;
	int PatchSize = int(Read - Start);
	uint8* TrampolinePtr = AllocateTrampoline(Start, PatchSize + TrampolineSize);

	// Write the trampoline
	*(void**)TrampolinePtr = HookFunction;

	uint8* PatchJmp = TrampolinePtr + sizeof(void*);
	memcpy(PatchJmp, Start, PatchSize);

	PatchJmp += PatchSize;
	*PatchJmp = 0xe9;
	*(int32*)(PatchJmp + 1) = int32(intptr_t(Start + PatchSize) - intptr_t(PatchJmp)) - 5;

	// Need to make the text section writeable
	DWORD ProtPrev;
	UPTRINT ProtBase = UPTRINT(Target) & ~0x0fff;						// 0x0fff is mask of VM page size
	SIZE_T ProtSize = ((ProtBase + 16 + 0x1000) & ~0x0fff) - ProtBase;	// 16 is enough for one x86 instruction
	VirtualProtect((void*)ProtBase, ProtSize, PAGE_EXECUTE_READWRITE, &ProtPrev);

	// Patch function to jmp to the hook
	uint16* HookJmp = (uint16*)Target;
	HookJmp[0] = 0x25ff;
	*(int32*)(HookJmp + 1) = int32(intptr_t(TrampolinePtr) - intptr_t(HookJmp + 3));

	// Put the protection back the way it was
	VirtualProtect((void*)ProtBase, ProtSize, ProtPrev, &ProtPrev);

	return PatchJmp - PatchSize;
}

////////////////////////////////////////////////////////////////////////////////
class FVirtualWinApiHooks
{
public:
	static void				Initialize(bool bInLight);

private:
							FVirtualWinApiHooks();
	static bool				bLight;
	static LPVOID WINAPI	VmAlloc(LPVOID Address, SIZE_T Size, DWORD Type, DWORD Protect);
	static LPVOID WINAPI	VmAllocEx(HANDLE Process, LPVOID Address, SIZE_T Size, DWORD Type, DWORD Protect);
#if (NTDDI_VERSION >= NTDDI_WIN10_RS4)
	static PVOID WINAPI		VmAlloc2(HANDLE Process, PVOID BaseAddress, SIZE_T Size, ULONG AllocationType,
								ULONG PageProtection, MEM_EXTENDED_PARAMETER* ExtendedParameters, ULONG ParameterCount);
	static PVOID(WINAPI* VmAlloc2Orig)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
	typedef PVOID(__stdcall* FnVirtualAlloc2)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
#else
	static LPVOID WINAPI	VmAlloc2(HANDLE Process, LPVOID BaseAddress, SIZE_T Size, ULONG AllocationType,
								ULONG PageProtection, void* ExtendedParameters, ULONG ParameterCount);
	static LPVOID(WINAPI* VmAlloc2Orig)(HANDLE, LPVOID, SIZE_T, ULONG, ULONG, /*MEM_EXTENDED_PARAMETER* */ void*, ULONG);
	typedef LPVOID(__stdcall* FnVirtualAlloc2)(HANDLE, LPVOID, SIZE_T, ULONG, ULONG, /* MEM_EXTENDED_PARAMETER* */ void*, ULONG);
#endif
	static BOOL WINAPI		VmFree(LPVOID Address, SIZE_T Size, DWORD Type);
	static BOOL WINAPI		VmFreeEx(HANDLE Process, LPVOID Address, SIZE_T Size, DWORD Type);
	static LPVOID			(WINAPI *VmAllocOrig)(LPVOID, SIZE_T, DWORD, DWORD);
	static LPVOID			(WINAPI *VmAllocExOrig)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
	static BOOL				(WINAPI *VmFreeOrig)(LPVOID, SIZE_T, DWORD);
	static BOOL				(WINAPI *VmFreeExOrig)(HANDLE, LPVOID, SIZE_T, DWORD);

};

////////////////////////////////////////////////////////////////////////////////
bool	FVirtualWinApiHooks::bLight;
LPVOID	(WINAPI *FVirtualWinApiHooks::VmAllocOrig)(LPVOID, SIZE_T, DWORD, DWORD);
LPVOID	(WINAPI *FVirtualWinApiHooks::VmAllocExOrig)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
#if (NTDDI_VERSION >= NTDDI_WIN10_RS4)
PVOID	(WINAPI* FVirtualWinApiHooks::VmAlloc2Orig)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
#else
LPVOID	(WINAPI* FVirtualWinApiHooks::VmAlloc2Orig)(HANDLE, LPVOID, SIZE_T, ULONG, ULONG, /*MEM_EXTENDED_PARAMETER* */ void*, ULONG);
#endif
BOOL	(WINAPI *FVirtualWinApiHooks::VmFreeOrig)(LPVOID, SIZE_T, DWORD);
BOOL	(WINAPI *FVirtualWinApiHooks::VmFreeExOrig)(HANDLE, LPVOID, SIZE_T, DWORD);

////////////////////////////////////////////////////////////////////////////////
void FVirtualWinApiHooks::Initialize(bool bInLight)
{
	bLight = bInLight;

	FTextSectionEditor Editor;

	// Note that hooking alloc functions is done last as applying the hook can
	// allocate some memory pages.

	VmFreeOrig = Editor.Hook(VirtualFree, &FVirtualWinApiHooks::VmFree);
	VmFreeExOrig = Editor.Hook(VirtualFreeEx, &FVirtualWinApiHooks::VmFreeEx);

#if PLATFORM_WINDOWS
#if (NTDDI_VERSION >= NTDDI_WIN10_RS4)
	{
		VmAlloc2Orig = Editor.Hook(VirtualAlloc2, &FVirtualWinApiHooks::VmAlloc2);
	}
#else // NTDDI_VERSION
	{
		VmAlloc2Orig = nullptr;
		HINSTANCE DllInstance;
		DllInstance = LoadLibrary(TEXT("kernelbase.dll"));
		if (DllInstance != NULL)
		{
#pragma warning(push)
#pragma warning(disable: 4191) // 'type cast': unsafe conversion from 'FARPROC' to 'FVirtualWinApiHooks::FnVirtualAlloc2'
			VmAlloc2Orig = (FnVirtualAlloc2)GetProcAddress(DllInstance, "VirtualAlloc2");
#pragma warning(pop)
			FreeLibrary(DllInstance);
		}
		if (VmAlloc2Orig)
		{
			VmAlloc2Orig = Editor.Hook(VmAlloc2Orig, &FVirtualWinApiHooks::VmAlloc2);
		}
	}
#endif // NTDDI_VERSION
#endif // PLATFORM_WINDOWS

	VmAllocExOrig = Editor.Hook(VirtualAllocEx, &FVirtualWinApiHooks::VmAllocEx);
	VmAllocOrig = Editor.Hook(VirtualAlloc, &FVirtualWinApiHooks::VmAlloc);
}

////////////////////////////////////////////////////////////////////////////////
LPVOID WINAPI FVirtualWinApiHooks::VmAlloc(LPVOID Address, SIZE_T Size, DWORD Type, DWORD Protect)
{
	LPVOID Ret = VmAllocOrig(Address, Size, Type, Protect);

	// Track any reserve for now. Going forward we need events to differentiate reserves/commits and
	// corresponding information on frees.
	if (Ret != nullptr &&
		((Type & MEM_RESERVE) || ((Type & MEM_COMMIT) && Address == nullptr)))
	{
		MemoryTrace_Alloc((uint64)Ret, Size, 0, EMemoryTraceRootHeap::SystemMemory);
		MemoryTrace_MarkAllocAsHeap((uint64)Ret, EMemoryTraceRootHeap::SystemMemory);
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI FVirtualWinApiHooks::VmFree(LPVOID Address, SIZE_T Size, DWORD Type)
{
	if (Type & MEM_RELEASE)
	{
		MemoryTrace_UnmarkAllocAsHeap((uint64)Address, EMemoryTraceRootHeap::SystemMemory);
		MemoryTrace_Free((uint64)Address, EMemoryTraceRootHeap::SystemMemory);
	}

	return VmFreeOrig(Address, Size, Type);
}

////////////////////////////////////////////////////////////////////////////////
LPVOID WINAPI FVirtualWinApiHooks::VmAllocEx(HANDLE Process, LPVOID Address, SIZE_T Size, DWORD Type, DWORD Protect)
{
	LPVOID Ret = VmAllocExOrig(Process, Address, Size, Type, Protect);

	if (Process == GetCurrentProcess() && Ret != nullptr &&
		((Type & MEM_RESERVE) || ((Type & MEM_COMMIT) && Address == nullptr)))
	{
		MemoryTrace_Alloc((uint64)Ret, Size, 0, EMemoryTraceRootHeap::SystemMemory);
		MemoryTrace_MarkAllocAsHeap((uint64)Ret, EMemoryTraceRootHeap::SystemMemory);
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI FVirtualWinApiHooks::VmFreeEx(HANDLE Process, LPVOID Address, SIZE_T Size, DWORD Type)
{
	if (Process == GetCurrentProcess() && (Type & MEM_RELEASE))
	{
		MemoryTrace_UnmarkAllocAsHeap((uint64)Address, EMemoryTraceRootHeap::SystemMemory);
		MemoryTrace_Free((uint64)Address, EMemoryTraceRootHeap::SystemMemory);
	}

	return VmFreeExOrig(Process, Address, Size, Type);
}

////////////////////////////////////////////////////////////////////////////////
#if (NTDDI_VERSION >= NTDDI_WIN10_RS4)
PVOID WINAPI FVirtualWinApiHooks::VmAlloc2(HANDLE Process, PVOID BaseAddress, SIZE_T Size, ULONG Type,
	ULONG PageProtection, MEM_EXTENDED_PARAMETER* ExtendedParameters, ULONG ParameterCount)
#else
LPVOID WINAPI FVirtualWinApiHooks::VmAlloc2(HANDLE Process, LPVOID BaseAddress, SIZE_T Size, ULONG Type,
	ULONG PageProtection, /*MEM_EXTENDED_PARAMETER* */ void* ExtendedParameters, ULONG ParameterCount)
#endif
{
	LPVOID Ret = VmAlloc2Orig(Process, BaseAddress, Size, Type, PageProtection, ExtendedParameters, ParameterCount);

	if (Process == GetCurrentProcess() && Ret != nullptr &&
		((Type & MEM_RESERVE) || ((Type & MEM_COMMIT) && BaseAddress == nullptr)))
	{
		MemoryTrace_Alloc((uint64)Ret, Size, 0, EMemoryTraceRootHeap::SystemMemory);
		MemoryTrace_MarkAllocAsHeap((uint64)Ret, EMemoryTraceRootHeap::SystemMemory);
	}

	return Ret;
}
#endif // defined(PLATFORM_SUPPORTS_TRACE_WIN32_VIRTUAL_MEMORY_HOOKS)

////////////////////////////////////////////////////////////////////////////////
FMalloc* MemoryTrace_Create(FMalloc* InMalloc)
{
	bool bEnabled = false;

	int ArgC = 0;
	const WIDECHAR* CmdLine = ::GetCommandLineW();
	const WIDECHAR* const* ArgV = ::CommandLineToArgvW(CmdLine, &ArgC);
	FMalloc* OutMalloc = MemoryTrace_CreateInternal(InMalloc, ArgC, ArgV);
	::LocalFree(HLOCAL(ArgV));

	if (OutMalloc != nullptr)
	{
#if defined(PLATFORM_SUPPORTS_TRACE_WIN32_VIRTUAL_MEMORY_HOOKS)
		FVirtualWinApiHooks::Initialize(false);
#endif

		return OutMalloc;
	}

	return InMalloc;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif // UE_MEMORY_TRACE_ENABLED
