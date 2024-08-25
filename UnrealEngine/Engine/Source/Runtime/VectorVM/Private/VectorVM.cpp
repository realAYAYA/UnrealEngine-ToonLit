// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorVM.h"
#include "VectorVMSerialization.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VectorVMPrivate.h"
#include "Stats/Stats.h"
#include "HAL/ConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Async/ParallelFor.h"
#include "Math/UnrealPlatformMathSSE.h"
#include <limits>

IMPLEMENT_MODULE(FDefaultModuleImpl, VectorVM);


DECLARE_STATS_GROUP(TEXT("VectorVM"), STATGROUP_VectorVM, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("VVM Execution"), STAT_VVMExec, STATGROUP_VectorVM);
DECLARE_CYCLE_STAT(TEXT("VVM Chunk"), STAT_VVMExecChunk, STATGROUP_VectorVM);

DEFINE_LOG_CATEGORY_STATIC(LogVectorVM, All, All);

//#define FREE_TABLE_LOCK_CONTENTION_WARNINGS (!UE_BUILD_SHIPPING)
#define FREE_TABLE_LOCK_CONTENTION_WARNINGS (0)

//I don't expect us to ever be waiting long
#define FREE_TABLE_LOCK_CONTENTION_WARN_THRESHOLD_MS (0.01)

#if UE_BUILD_DEBUG
#define VM_FORCEINLINE FORCENOINLINE
#else
#define VM_FORCEINLINE FORCEINLINE
#endif

#define OP_REGISTER (0)
#define OP0_CONST (1 << 0)
#define OP1_CONST (1 << 1)
#define OP2_CONST (1 << 2)

#define SRCOP_RRR (OP_REGISTER | OP_REGISTER | OP_REGISTER)
#define SRCOP_RRC (OP_REGISTER | OP_REGISTER | OP0_CONST)
#define SRCOP_RCR (OP_REGISTER | OP1_CONST | OP_REGISTER)
#define SRCOP_RCC (OP_REGISTER | OP1_CONST | OP0_CONST)
#define SRCOP_CRR (OP2_CONST | OP_REGISTER | OP_REGISTER)
#define SRCOP_CRC (OP2_CONST | OP_REGISTER | OP0_CONST)
#define SRCOP_CCR (OP2_CONST | OP1_CONST | OP_REGISTER)
#define SRCOP_CCC (OP2_CONST | OP1_CONST | OP0_CONST)

namespace VectorVMConstants
{
	static const VectorRegister4Int VectorStride = MakeVectorRegisterInt(VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS);

	// for generating shuffle masks given input {A, B, C, D}
	constexpr uint32 ShufMaskIgnore = 0xFFFFFFFF;
	constexpr uint32 ShufMaskA = 0x03020100;
	constexpr uint32 ShufMaskB = 0x07060504;
	constexpr uint32 ShufMaskC = 0x0B0A0908;
	constexpr uint32 ShufMaskD = 0x0F0E0D0C;

	static const VectorRegister4Int RegisterShuffleMask[] =
	{
		MakeVectorRegisterInt(ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore), // 0000
		MakeVectorRegisterInt(ShufMaskA, ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore), // 0001
		MakeVectorRegisterInt(ShufMaskB, ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore), // 0010
		MakeVectorRegisterInt(ShufMaskA, ShufMaskB, ShufMaskIgnore, ShufMaskIgnore), // 0011
		MakeVectorRegisterInt(ShufMaskC, ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore), // 0100
		MakeVectorRegisterInt(ShufMaskA, ShufMaskC, ShufMaskIgnore, ShufMaskIgnore), // 0101
		MakeVectorRegisterInt(ShufMaskB, ShufMaskC, ShufMaskIgnore, ShufMaskIgnore), // 0110
		MakeVectorRegisterInt(ShufMaskA, ShufMaskB, ShufMaskC, ShufMaskIgnore), // 0111
		MakeVectorRegisterInt(ShufMaskD, ShufMaskIgnore, ShufMaskIgnore, ShufMaskIgnore), // 1000
		MakeVectorRegisterInt(ShufMaskA, ShufMaskD, ShufMaskIgnore, ShufMaskIgnore), // 1001
		MakeVectorRegisterInt(ShufMaskB, ShufMaskD, ShufMaskIgnore, ShufMaskIgnore), // 1010
		MakeVectorRegisterInt(ShufMaskA, ShufMaskB, ShufMaskD, ShufMaskIgnore), // 1011
		MakeVectorRegisterInt(ShufMaskC, ShufMaskD, ShufMaskIgnore, ShufMaskIgnore), // 1100
		MakeVectorRegisterInt(ShufMaskA, ShufMaskC, ShufMaskD, ShufMaskIgnore), // 1101
		MakeVectorRegisterInt(ShufMaskB, ShufMaskC, ShufMaskD, ShufMaskIgnore), // 1110
		MakeVectorRegisterInt(ShufMaskA, ShufMaskB, ShufMaskC, ShufMaskD), // 1111
	};
};

#define VM_USE_ACCURATE_VECTOR_FUNCTIONS (1)

namespace VectorVMAccuracy
{
	VM_FORCEINLINE static VectorRegister4Float Reciprocal(const VectorRegister4Float& Src)
	{
#if VM_USE_ACCURATE_VECTOR_FUNCTIONS
		return VectorReciprocalAccurate(Src);
#else
		return VectorReciprocal(Src);
#endif
	}

	VM_FORCEINLINE static VectorRegister4Float ReciprocalSqrt(const VectorRegister4Float& Src)
	{
#if VM_USE_ACCURATE_VECTOR_FUNCTIONS
		return VectorReciprocalSqrtAccurate(Src);
#else
		return VectorReciprocalSqrt(Src);
#endif
	}

	VM_FORCEINLINE static VectorRegister4Float Sqrt(const VectorRegister4Float& Src)
	{
		return Reciprocal(ReciprocalSqrt(Src));
	}
};

// helper function wrapping the SSE3 shuffle operation.  Currently implemented for some platforms, the
// rest will just use the FPU version so as to not push the requirements up to SSE3 (currently SSE2)
#if PLATFORM_ENABLE_VECTORINTRINSICS && PLATFORM_ALWAYS_HAS_SSE4_1
#define VectorIntShuffle( Vec, Mask )	_mm_shuffle_epi8( (Vec), (Mask) )
#elif PLATFORM_ENABLE_VECTORINTRINSICS_NEON
/**
 * Shuffles a VectorInt using a provided shuffle mask
 *
 * @param Vec		Source vector
 * @param Mask		Shuffle vector
 */
FORCEINLINE VectorRegister4Int VectorIntShuffle(const VectorRegister4Int& Vec, const VectorRegister4Int& Mask)
{
	uint8x8x2_t VecSplit = { { vget_low_u8(Vec), vget_high_u8(Vec) } };
	return vcombine_u8(vtbl2_u8(VecSplit, vget_low_u8(Mask)), vtbl2_u8(VecSplit, vget_high_u8(Mask)));
}

#else
FORCEINLINE VectorRegister4Int VectorIntShuffle(const VectorRegister4Int& Vec, const VectorRegister4Int& Mask)
{
	VectorRegister4Int Result;
	const int8* VecBytes = reinterpret_cast<const int8*>(&Vec);
	const int8* MaskBytes = reinterpret_cast<const int8*>(&Mask);
	int8* ResultBytes = reinterpret_cast<int8*>(&Result);

	for (int32 i = 0; i < sizeof(VectorRegister4Int); ++i)
	{
		ResultBytes[i] = (MaskBytes[i] < 0) ? 0 : VecBytes[MaskBytes[i] % 16];
	}

	return Result;
}
#endif

#if VECTORVM_SUPPORTS_LEGACY

//Temporarily locking the free table until we can implement a lock free algorithm. UE-65856
FORCEINLINE void FDataSetMeta::LockFreeTable()
{
#if FREE_TABLE_LOCK_CONTENTION_WARNINGS
	uint64 StartCycles = FPlatformTime::Cycles64();
#endif
 		
	FreeTableLock.Lock();
 
#if FREE_TABLE_LOCK_CONTENTION_WARNINGS
	uint64 EndCylces = FPlatformTime::Cycles64();
	double DurationMs = FPlatformTime::ToMilliseconds64(EndCylces - StartCycles);
	if (DurationMs >= FREE_TABLE_LOCK_CONTENTION_WARN_THRESHOLD_MS)
	{
		UE_LOG(LogVectorVM, Warning, TEXT("VectorVM Stalled in LockFreeTable()! %g ms"), DurationMs);
	}
#endif
}

FORCEINLINE void FDataSetMeta::UnlockFreeTable()
{
 	FreeTableLock.Unlock();
}

#endif //VECTORVM_SUPPORTS_LEGACY

static int32 GbParallelVVM = 1;
static FAutoConsoleVariableRef CVarbParallelVVM(
	TEXT("vm.Parallel"),
	GbParallelVVM,
	TEXT("If > 0 vector VM chunk level paralellism will be enabled. \n"),
	ECVF_Default
);

static int32 GParallelVVMChunksPerBatch = 4;
static FAutoConsoleVariableRef CVarParallelVVMChunksPerBatch(
	TEXT("vm.ParallelChunksPerBatch"),
	GParallelVVMChunksPerBatch,
	TEXT("Number of chunks to process per task when running in parallel. \n"),
	ECVF_Default
);

//These are possibly too granular to enable for everyone.
static int32 GbDetailedVMScriptStats = 0;
static FAutoConsoleVariableRef CVarDetailedVMScriptStats(
	TEXT("vm.DetailedVMScriptStats"),
	GbDetailedVMScriptStats,
	TEXT("If > 0 the vector VM will emit stats for it's internal module calls. \n"),
	ECVF_Default
);

static int32 GParallelVVMInstancesPerChunk = 128;
static FAutoConsoleVariableRef CVarParallelVVMInstancesPerChunk(
	TEXT("vm.InstancesPerChunk"),
	GParallelVVMInstancesPerChunk,
	TEXT("Number of instances per VM chunk. (default=128) \n"),
	ECVF_ReadOnly
);

static int32 GVVMChunkSizeInBytes = 32768;
static FAutoConsoleVariableRef CVarVVMChunkSizeInBytes(
	TEXT("vm.ChunkSizeInBytes"),
	GVVMChunkSizeInBytes,
	TEXT("Number of bytes per VM chunk  Ideally <= L1 size. (default=32768) \n"),
	ECVF_Default
);

static int32 GVVMMaxThreadsPerScript = 8;
static FAutoConsoleVariableRef CVarVVMMaxThreadsPerScript(
	TEXT("vm.MaxThreadsPerScript"),
	GVVMMaxThreadsPerScript,
	TEXT("Maximum number of threads per script. Set 0 to mean 'as many as necessary'\n"),
	ECVF_Default
);

static int32 GbOptimizeVMByteCode = 1;
static FAutoConsoleVariableRef CVarbOptimizeVMByteCode(
	TEXT("vm.OptimizeVMByteCode"),
	GbOptimizeVMByteCode,
	TEXT("If > 0 vector VM code optimization will be enabled at runtime.\n"),
	ECVF_Default
);

static int32 GbFreeUnoptimizedVMByteCode = 1;
static FAutoConsoleVariableRef CVarbFreeUnoptimizedVMByteCode(
	TEXT("vm.FreeUnoptimizedByteCode"),
	GbFreeUnoptimizedVMByteCode,
	TEXT("When we have optimized the VM byte code should we free the original unoptimized byte code?"),
	ECVF_Default
);

static int32 GbUseOptimizedVMByteCode = 1;
static FAutoConsoleVariableRef CVarbUseOptimizedVMByteCode(
	TEXT("vm.UseOptimizedVMByteCode"),
	GbUseOptimizedVMByteCode,
	TEXT("If > 0 optimized vector VM code will be excuted at runtime.\n"),
	ECVF_Default
);

static int32 GbSafeOptimizedKernels = 1;
static FAutoConsoleVariableRef CVarbSafeOptimizedKernels(
	TEXT("vm.SafeOptimizedKernels"),
	GbSafeOptimizedKernels,
	TEXT("If > 0 optimized vector VM byte code will use safe versions of the kernels.\n"),
	ECVF_Default
);

static int32 GbBatchVMInput = 0;
static FAutoConsoleVariableRef CVarBatchVMInput(
	TEXT("vm.BatchVMInput"),
	GbBatchVMInput,
	TEXT("If > 0 input elements will be batched.\n"),
	ECVF_Default
);

static int32 GbBatchVMOutput = 0;
static FAutoConsoleVariableRef CVarBatchVMOutput(
	TEXT("vm.BatchVMOutput"),
	GbBatchVMOutput,
	TEXT("If > 0 output elements will be batched.\n"),
	ECVF_Default
);

static int32 GbBatchPackVMOutput = 1;
static FAutoConsoleVariableRef CVarbBatchPackVMOutput(
	TEXT("vm.BatchPackedVMOutput"),
	GbBatchPackVMOutput,
	TEXT("If > 0 output elements will be packed and batched branch free.\n"),
	ECVF_Default
);

uint8 VectorVM::GetNumOpCodes()
{
	return (uint8)EVectorVMOp::NumOpcodes;
}

#if WITH_EDITOR
//UEnum* g_VectorVMEnumStateObj = nullptr;
UEnum* g_VectorVMEnumOperandObj = nullptr;

#define VVM_OP_XM(n, ...) #n,
static const char *VVM_OP_NAMES[] {
	VVM_OP_XM_LIST
};
#undef VVM_OP_XM

FString VectorVM::GetOpName(EVectorVMOp Op)
{
	//check(g_VectorVMEnumStateObj);
	//
	//FString OpStr = g_VectorVMEnumStateObj->GetNameByValue((uint8)Op).ToString();
	//int32 LastIdx = 0;
	//OpStr.FindLastChar(TEXT(':'), LastIdx);
	//return OpStr.RightChop(LastIdx + 1);

	int OpIdx = (int)Op;
	if (OpIdx < 0 || OpIdx >= (int)EVectorVMOp::NumOpcodes) {
		OpIdx = 0;
	}
	FString OpStr(VVM_OP_NAMES[OpIdx]);
	return OpStr;
}

FString VectorVM::GetOperandLocationName(EVectorVMOperandLocation Location)
{
	check(g_VectorVMEnumOperandObj);

	FString LocStr = g_VectorVMEnumOperandObj->GetNameByValue((uint8)Location).ToString();
	int32 LastIdx = 0;
	LocStr.FindLastChar(TEXT(':'), LastIdx);
	return LocStr.RightChop(LastIdx + 1);
}
#endif

uint8 VectorVM::CreateSrcOperandMask(EVectorVMOperandLocation Type0, EVectorVMOperandLocation Type1, EVectorVMOperandLocation Type2)
{
	return	(Type0 == EVectorVMOperandLocation::Constant ? OP0_CONST : OP_REGISTER) |
		(Type1 == EVectorVMOperandLocation::Constant ? OP1_CONST : OP_REGISTER) |
		(Type2 == EVectorVMOperandLocation::Constant ? OP2_CONST : OP_REGISTER);
}

#if VECTORVM_SUPPORTS_LEGACY
namespace VectorKernelNoiseImpl
{
	static void BuildNoiseTable();
}
#endif

void VectorVM::Init()
{
	static bool Inited = false;
	if (Inited == false)
	{
#if WITH_EDITOR
		//g_VectorVMEnumStateObj = StaticEnum<EVectorVMOp>();
		g_VectorVMEnumOperandObj = StaticEnum<EVectorVMOperandLocation>();
#endif

#if VECTORVM_SUPPORTS_LEGACY
		VectorKernelNoiseImpl::BuildNoiseTable();
#endif
		Inited = true;
	}
}

#if VECTORVM_SUPPORTS_LEGACY

//////////////////////////////////////////////////////////////////////////
//  VM Code Optimizer Context

typedef void(*FVectorVMExecFunction)(FVectorVMContext&);

struct FVectorVMCodeOptimizerContext
{
	typedef bool(*OptimizeVMFunction)(EVectorVMOp, FVectorVMCodeOptimizerContext&);

	explicit FVectorVMCodeOptimizerContext(FVectorVMContext& InBaseContext, const uint8* ByteCode, TArray<uint8>& InOptimizedCode, TArrayView<uint8> InExternalFunctionRegisterCounts)
		: BaseContext(InBaseContext)
		, OptimizedCode(InOptimizedCode)
		, ExternalFunctionRegisterCounts(InExternalFunctionRegisterCounts)
	{
		BaseContext.PrepareForExec(0, 0, nullptr, nullptr, nullptr, nullptr, TArrayView<FDataSetMeta>(), 0, false);
		BaseContext.PrepareForChunk(ByteCode, 0, 0);

		// write out a jump table offset that we'll fill in when the table is encoded (EncodeJumpTable())
		Write<uint32>(0);
	}
	FVectorVMCodeOptimizerContext(const FVectorVMCodeOptimizerContext&) = delete;
	FVectorVMCodeOptimizerContext(const FVectorVMCodeOptimizerContext&&) = delete;

	template<uint32 InstancesPerOp>
	int32 GetNumLoops() const { return 0; }

	FORCEINLINE EVectorVMOp PeekOp()
	{
		return static_cast<EVectorVMOp>(*BaseContext.Code);
	}

	FORCEINLINE uint8 DecodeU8() { return BaseContext.DecodeU8(); }
	FORCEINLINE uint16 DecodeU16() { return BaseContext.DecodeU16(); }
	FORCEINLINE uint32 DecodeU32() { return BaseContext.DecodeU32(); }
	FORCEINLINE uint64 DecodeU64() { return BaseContext.DecodeU64(); }

	//-TODO: Support unaligned writes
	template<typename T>
	void Write(const T& v)
	{
		reinterpret_cast<T&>(OptimizedCode[OptimizedCode.AddUninitialized(sizeof(T))]) = v;
	}

	void WriteExecFunction(const FVectorVMExecFunction& Function)
	{
		const int32 JumpTableIndex = JumpTable.AddUnique(Function);
		check(JumpTableIndex <= TNumericLimits<uint8>::Max());
		Write<uint8>(JumpTableIndex);
	}

	struct FOptimizerCodeState
	{
		uint8 const* BaseContextCode;
		int32 OptimizedCodeLength;
	};

	FOptimizerCodeState CreateCodeState()
	{
		FOptimizerCodeState State;
		State.BaseContextCode = BaseContext.Code;
		State.OptimizedCodeLength = OptimizedCode.Num();

		return State;
	}

	void RollbackCodeState(const FOptimizerCodeState& State)
	{
		BaseContext.Code = State.BaseContextCode;
		OptimizedCode.SetNum(State.OptimizedCodeLength, EAllowShrinking::No);
	}

	// Jump table is encoded at the end of the optimized code, with the first int32 in the byte code
	// stream being the start offset

	static const FVectorVMExecFunction* DecodeJumpTable(const uint8*& OptimizedByteCode)
	{
		const uint32 JumpTableOffset = *reinterpret_cast<const uint32*>(OptimizedByteCode);
		const FVectorVMExecFunction* JumpTable = reinterpret_cast<const FVectorVMExecFunction*>(OptimizedByteCode + JumpTableOffset);

		OptimizedByteCode += sizeof(uint32);

		return JumpTable;
	}

	void EncodeJumpTable()
	{
		const int32 JumpTableCount = JumpTable.Num();
		check(JumpTableCount > 0);
		check(JumpTableCount <= TNumericLimits<uint8>::Max());

		if (JumpTableCount <= 0 || JumpTableCount > TNumericLimits<uint8>::Max())
		{
			// if the jump table is too big, then clear out the OptimizedCode and we'll just have to run the unoptimized path
			OptimizedCode.Reset();
			return;
		}

		// write the offset to the jump table into the reserved slot
		const uint32 JumpTableOffset = OptimizedCode.Num();
		*reinterpret_cast<uint32*>(OptimizedCode.GetData()) = JumpTableOffset;

		const int32 JumpTableSize = JumpTableCount * sizeof(FVectorVMExecFunction);
		OptimizedCode.AddUninitialized(JumpTableSize);
		FMemory::Memcpy(OptimizedCode.GetData() + JumpTableOffset, JumpTable.GetData(), JumpTableSize);
	}

	FVectorVMContext&		BaseContext;
	TArray<uint8>&			OptimizedCode;
	TArray<FVectorVMExecFunction, TInlineAllocator<256>> JumpTable;
	const TArrayView<uint8>	ExternalFunctionRegisterCounts;
	const int32				StartInstance = 0;
};

//////////////////////////////////////////////////////////////////////////
//  Constant Handlers

struct FConstantHandlerBase
{
	const uint16 ConstantIndex;
	FConstantHandlerBase(FVectorVMContext& Context)
		: ConstantIndex(Context.DecodeU16())
	{}

	FORCEINLINE void Advance() { }

	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.Write(Context.DecodeU16());
	}

	static void OptimizeSkip(FVectorVMCodeOptimizerContext& Context)
	{
		Context.DecodeU16();
	}
};

template<typename T>
struct FConstantHandler : public FConstantHandlerBase
{
	const T Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(*Context.GetConstant<T>(ConstantIndex))
	{
	}

	FORCEINLINE const T& Get() const { return Constant; }
	FORCEINLINE const T& GetAndAdvance() { return Constant; }
};

template<>
struct FConstantHandler<VectorRegister4Float> : public FConstantHandlerBase
{
	static VectorRegister4Float LoadConstant(const FVectorVMContext& Context, uint16 ConstantIndex)
	{
		float ConstantValue = *Context.GetConstant<float>(ConstantIndex);

		return MakeVectorRegister(ConstantValue, ConstantValue, ConstantValue, ConstantValue);
	}

	const VectorRegister4Float Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(LoadConstant(Context, ConstantIndex))
	{}

	FORCEINLINE const VectorRegister4Float Get() const { return Constant; }
	FORCEINLINE const VectorRegister4Float GetAndAdvance() { return Constant; }
};

template<>
struct FConstantHandler<VectorRegister4Int> : public FConstantHandlerBase
{
	static VectorRegister4Int LoadConstant(const FVectorVMContext& Context, uint16 ConstantIndex)
	{
		int32 ConstantValue = *Context.GetConstant<int32>(ConstantIndex);

		return MakeVectorRegisterInt(ConstantValue, ConstantValue, ConstantValue, ConstantValue);
	}

	const VectorRegister4Int Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(LoadConstant(Context, ConstantIndex))
	{}

	FORCEINLINE const VectorRegister4Int Get() const { return Constant; }
	FORCEINLINE const VectorRegister4Int GetAndAdvance() { return Constant; }
};


//////////////////////////////////////////////////////////////////////////
// Register handlers.
// Handle reading of a register, advancing the pointer with each read.

struct FRegisterHandlerBase
{
	const int32 RegisterIndex;
	FORCEINLINE FRegisterHandlerBase(FVectorVMContext& Context)
		: RegisterIndex(Context.DecodeU16())
	{}

	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.Write(Context.DecodeU16());
	}
};

template<typename T>
struct FRegisterHandler : public FRegisterHandlerBase
{
private:
	T * RESTRICT Register;
public:
	FORCEINLINE FRegisterHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register((T*)Context.GetTempRegister(RegisterIndex))
	{}

	FORCEINLINE const T Get() { return *Register; }
	FORCEINLINE T* GetDest() { return Register; }
	FORCEINLINE void Advance() { ++Register; }
	FORCEINLINE const T GetAndAdvance()
	{
		return *Register++;
	}
	FORCEINLINE T* GetDestAndAdvance()
	{
		return Register++;
	}
};

//////////////////////////////////////////////////////////////////////////

FVectorVMContext::FVectorVMContext()
	: Code(nullptr)
	, ConstantTable(nullptr)
	, ExternalFunctionTable(nullptr)
	, UserPtrTable(nullptr)
	, NumInstances(0)
	, StartInstance(0)
	, TempRegisterSize(0)
	, TempBufferSize(0)
{
	RandStream.GenerateNewSeed();
}

void FVectorVMContext::PrepareForExec(
	int32 InNumTempRegisters,
	int32 InConstantTableCount,
	const uint8* const* InConstantTable,
	const int32* InConstantTableSizes,
	const FVMExternalFunction* const* InExternalFunctionTable,
	void** InUserPtrTable,
	TArrayView<FDataSetMeta> InDataSetMetaTable,
	int32 MaxNumInstances,
	bool bInParallelExecution
)
{
	NumTempRegisters = InNumTempRegisters;
	ConstantTableCount = InConstantTableCount;
	ConstantTableSizes = InConstantTableSizes;
	ConstantTable = InConstantTable;
	ExternalFunctionTable = InExternalFunctionTable;
	UserPtrTable = InUserPtrTable;

	TempRegisterSize = Align(MaxNumInstances * VectorVM::MaxInstanceSizeBytes, PLATFORM_CACHE_LINE_SIZE);
	TempBufferSize = TempRegisterSize * NumTempRegisters;
	TempRegTable.SetNumUninitialized(TempBufferSize, EAllowShrinking::No);

	DataSetMetaTable = InDataSetMetaTable;

	for (auto& TLSTempData : ThreadLocalTempData)
	{
		TLSTempData.Reset();
	}
	ThreadLocalTempData.SetNum(DataSetMetaTable.Num());

	bIsParallelExecution = bInParallelExecution;
}

#if STATS
void FVectorVMContext::SetStatScopes(TArrayView<FStatScopeData> InStatScopes)
{
	StatScopes = InStatScopes;
	StatCounterStack.Reserve(StatScopes.Num());
	ScopeExecCycles.Reset(StatScopes.Num());
	ScopeExecCycles.AddZeroed(StatScopes.Num());
}
#elif ENABLE_STATNAMEDEVENTS
void FVectorVMContext::SetStatNamedEventScopes(TArrayView<const FString> InStatNamedEventScopes)
{
	StatNamedEventScopes = InStatNamedEventScopes;
}
#endif

void FVectorVMContext::FinishExec()
{
	//At the end of executing each chunk we can push any thread local temporary data out to the main storage with locks or atomics.

	check(ThreadLocalTempData.Num() == DataSetMetaTable.Num());
	for(int32 DataSetIndex=0; DataSetIndex < DataSetMetaTable.Num(); ++DataSetIndex)
	{
		FDataSetThreadLocalTempData&RESTRICT Data = ThreadLocalTempData[DataSetIndex];

		if (Data.IDsToFree.Num() > 0)
		{
			TArray<int32>&RESTRICT FreeIDTable = *DataSetMetaTable[DataSetIndex].FreeIDTable;
			int32&RESTRICT NumFreeIDs = *DataSetMetaTable[DataSetIndex].NumFreeIDs;
			check(FreeIDTable.Num() >= NumFreeIDs + Data.IDsToFree.Num());

			//Temporarily locking the free table until we can implement something lock-free
			DataSetMetaTable[DataSetIndex].LockFreeTable();
			for (int32 IDToFree : Data.IDsToFree)
			{
				//UE_LOG(LogVectorVM, Warning, TEXT("AddFreeID: ID:%d | FreeTableIdx:%d."), IDToFree, NumFreeIDs);
				FreeIDTable[NumFreeIDs++] = IDToFree;
			}
			//Unlock the free table.
			DataSetMetaTable[DataSetIndex].UnlockFreeTable();
			Data.IDsToFree.Reset();
		}

		//Also update the max ID seen. This should be the ONLY place in the VM we update this max value.
		if ( bIsParallelExecution )
		{
			volatile int32* MaxUsedID = DataSetMetaTable[DataSetIndex].MaxUsedID;
			int32 LocalMaxUsedID;
			do
			{
				LocalMaxUsedID = *MaxUsedID;
				if (LocalMaxUsedID >= Data.MaxID)
				{
					break;
				}
			} while (FPlatformAtomics::InterlockedCompareExchange(MaxUsedID, Data.MaxID, LocalMaxUsedID) != LocalMaxUsedID);

			*MaxUsedID = FMath::Max(*MaxUsedID, Data.MaxID);
		}
		else
		{
			int32* MaxUsedID = DataSetMetaTable[DataSetIndex].MaxUsedID;
			*MaxUsedID = FMath::Max(*MaxUsedID, Data.MaxID);
		}
	}

#if STATS
	check(ScopeExecCycles.Num() == StatScopes.Num());
	for (int i = 0; i < StatScopes.Num(); i++)
	{
		uint64 ExecTime = ScopeExecCycles[i];
		if (ExecTime > 0)
		{
			std::atomic_fetch_add(&StatScopes[i].ExecutionCycleCount, ExecTime);
		}
	}
	StatScopes = TArrayView<FStatScopeData>();
#elif ENABLE_STATNAMEDEVENTS
	StatNamedEventScopes = TArrayView<FString>();
#endif
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Kernels
template<typename Kernel, typename DstHandler, typename Arg0Handler, uint32 NumInstancesPerOp>
struct TUnaryKernelHandler
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Arg0Handler::Optimize(Context);
		DstHandler::Optimize(Context);
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context);
		DstHandler Dst(Context);

		const int32 Loops = Context.GetNumLoops<NumInstancesPerOp>();
		for (int32 i = 0; i < Loops; ++i)
		{
			Kernel::DoKernel(Context, Dst.GetDestAndAdvance(), Arg0.GetAndAdvance());
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, int32 NumInstancesPerOp>
struct TBinaryKernelHandler
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Arg0Handler::Optimize(Context);
		Arg1Handler::Optimize(Context);
		DstHandler::Optimize(Context);
	}

	static void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context); 
		Arg1Handler Arg1(Context);

		DstHandler Dst(Context);

		const int32 Loops = Context.GetNumLoops<NumInstancesPerOp>();
		for (int32 i = 0; i < Loops; ++i)
		{
			Kernel::DoKernel(Context, Dst.GetDestAndAdvance(), Arg0.GetAndAdvance(), Arg1.GetAndAdvance());
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler, int32 NumInstancesPerOp>
struct TTrinaryKernelHandler
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Arg0Handler::Optimize(Context);
		Arg1Handler::Optimize(Context);
		Arg2Handler::Optimize(Context);
		DstHandler::Optimize(Context);
	}

	static void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context);
		Arg1Handler Arg1(Context);
		Arg2Handler Arg2(Context);

		DstHandler Dst(Context);

		const int32 Loops = Context.GetNumLoops<NumInstancesPerOp>();
		for (int32 i = 0; i < Loops; ++i)
		{
			Kernel::DoKernel(Context, Dst.GetDestAndAdvance(), Arg0.GetAndAdvance(), Arg1.GetAndAdvance(), Arg2.GetAndAdvance());
		}
	}
};


/** Base class of vector kernels with a single operand. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, int32 NumInstancesPerOp>
struct TUnaryKernel
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpTypes = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TUnaryKernelHandler<Kernel, DstHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RRC:	TUnaryKernelHandler<Kernel, DstHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		default: check(0); break;
		};
	}

	static void Exec(FVectorVMContext& Context)
	{
		const uint32 SrcOpTypes = Context.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TUnaryKernelHandler<Kernel, DstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TUnaryKernelHandler<Kernel, DstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break; 
		};
	}
};
template<typename Kernel>
struct TUnaryScalarKernel : public TUnaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TUnaryVectorKernel : public TUnaryKernel<Kernel, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TUnaryScalarIntKernel : public TUnaryKernel<Kernel, FRegisterHandler<int32>, FConstantHandler<int32>, FRegisterHandler<int32>, 1> {};
template<typename Kernel>
struct TUnaryVectorIntKernel : public TUnaryKernel<Kernel, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS> {};

/** Base class of Vector kernels with 2 operands. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, uint32 NumInstancesPerOp>
struct TBinaryKernel
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpTypes = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RRC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RCR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RCC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		default: check(0); break;
		};
	}

	static void Exec(FVectorVMContext& Context)
	{
		const uint32 SrcOpTypes = Context.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break;
		};
	}
};
template<typename Kernel>
struct TBinaryScalarKernel : public TBinaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TBinaryVectorKernel : public TBinaryKernel<Kernel, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TBinaryVectorIntKernel : public TBinaryKernel<Kernel, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS> {};

/** Base class of Vector kernels with 3 operands. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, uint32 NumInstancesPerOp>
struct TTrinaryKernel
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpTypes = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_RCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_CRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_CRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_CCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		case SRCOP_CCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Optimize(Context); break;
		default: check(0); break;
		};
	}

	static void Exec(FVectorVMContext& Context)
	{
		const uint32 SrcOpTypes = Context.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break;
		};
	}
};

template<typename Kernel>
struct TTrinaryScalarKernel : public TTrinaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TTrinaryVectorKernel : public TTrinaryKernel<Kernel, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TTrinaryVectorIntKernel : public TTrinaryKernel<Kernel, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS> {};


/*------------------------------------------------------------------------------
	Implementation of all kernel operations.
------------------------------------------------------------------------------*/

struct FVectorKernelAdd : public TBinaryVectorKernel<FVectorKernelAdd>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorAdd(Src0, Src1);
	}
};

struct FVectorKernelSub : public TBinaryVectorKernel<FVectorKernelSub>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorSubtract(Src0, Src1);
	}
};

struct FVectorKernelMul : public TBinaryVectorKernel<FVectorKernelMul>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorMultiply(Src0, Src1);
	}
};

struct FVectorKernelDiv : public TBinaryVectorKernel<FVectorKernelDiv>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorDivide(Src0, Src1);
	}
};

struct FVectorKernelDivSafe : public TBinaryVectorKernel<FVectorKernelDivSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(VectorAbs(Src1), GlobalVectorConstants::SmallNumber);
		*Dst = VectorSelect(ValidMask, VectorDivide(Src0, Src1), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelMad : public TTrinaryVectorKernel<FVectorKernelMad>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1,VectorRegister4Float Src2)
	{
		*Dst = VectorMultiplyAdd(Src0, Src1, Src2);
	}
};

struct FVectorKernelLerp : public TTrinaryVectorKernel<FVectorKernelLerp>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1,VectorRegister4Float Src2)
	{
		const VectorRegister4Float OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Src2);
		const VectorRegister4Float Tmp = VectorMultiply(Src0, OneMinusAlpha);
		*Dst = VectorMultiplyAdd(Src1, Src2, Tmp);
	}
};

struct FVectorKernelRcp : public TUnaryVectorKernel<FVectorKernelRcp>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0)
	{
		*Dst = VectorVMAccuracy::Reciprocal(Src0);
	}
};

// if the magnitude of the value is too small, then the result will be 0 (not NaN/Inf)
struct FVectorKernelRcpSafe : public TUnaryVectorKernel<FVectorKernelRcpSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(VectorAbs(Src0), GlobalVectorConstants::SmallNumber);
		*Dst = VectorSelect(ValidMask, VectorVMAccuracy::Reciprocal(Src0), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelRsq : public TUnaryVectorKernel<FVectorKernelRsq>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0)
	{
		*Dst = VectorVMAccuracy::ReciprocalSqrt(Src0);
	}
};

// if the value is very small or negative, then the result will be 0 (not NaN/Inf/imaginary)
struct FVectorKernelRsqSafe : public TUnaryVectorKernel<FVectorKernelRsqSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(Src0, GlobalVectorConstants::SmallNumber);
		*Dst = VectorSelect(ValidMask, VectorVMAccuracy::ReciprocalSqrt(Src0), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelSqrt : public TUnaryVectorKernel<FVectorKernelSqrt>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0)
	{
		*Dst = VectorVMAccuracy::Sqrt(Src0);
	}
};

struct FVectorKernelSqrtSafe : public TUnaryVectorKernel<FVectorKernelSqrtSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(Src0, GlobalVectorConstants::SmallNumber);
		*Dst = VectorSelect(ValidMask, VectorVMAccuracy::Sqrt(Src0), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelNeg : public TUnaryVectorKernel<FVectorKernelNeg>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0)
	{
		*Dst = VectorNegate(Src0);
	}
};

struct FVectorKernelAbs : public TUnaryVectorKernel<FVectorKernelAbs>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0)
	{
		*Dst = VectorAbs(Src0);
	}
};

struct FVectorKernelExp : public TUnaryVectorKernel<FVectorKernelExp>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorExp(Src0);
	}
}; 

struct FVectorKernelExp2 : public TUnaryVectorKernel<FVectorKernelExp2>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorExp2(Src0);
	}
};

struct FVectorKernelLog : public TUnaryVectorKernel<FVectorKernelLog>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorLog(Src0);
	}
};

struct FVectorKernelLogSafe : public TUnaryVectorKernel<FVectorKernelLogSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(Src0, GlobalVectorConstants::FloatZero);

		*Dst = VectorSelect(ValidMask, VectorLog(Src0), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelLog2 : public TUnaryVectorKernel<FVectorKernelLog2>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorLog2(Src0);
	}
};

struct FVectorKernelStep : public TBinaryVectorKernel<FVectorKernelStep>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorStep(VectorSubtract(Src1, Src0));
	}
};

struct FVectorKernelClamp : public TTrinaryVectorKernel<FVectorKernelClamp>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1,VectorRegister4Float Src2)
	{
		const VectorRegister4Float Tmp = VectorMax(Src0, Src1);
		*Dst = VectorMin(Tmp, Src2);
	}
};

struct FVectorKernelSin : public TUnaryVectorKernel<FVectorKernelSin>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorSin(Src0);
	}
};

struct FVectorKernelCos : public TUnaryVectorKernel<FVectorKernelCos>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
 	{
		*Dst = VectorCos(Src0);
	}
};

struct FVectorKernelTan : public TUnaryVectorKernel<FVectorKernelTan>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorTan(Src0);
	}
};

struct FVectorKernelASin : public TUnaryVectorKernel<FVectorKernelASin>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorASin(Src0);
	}
};

struct FVectorKernelACos : public TUnaryVectorKernel<FVectorKernelACos>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorACos(Src0);
	}
};

struct FVectorKernelATan : public TUnaryVectorKernel<FVectorKernelATan>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorATan(Src0);
	}
};

struct FVectorKernelATan2 : public TBinaryVectorKernel<FVectorKernelATan2>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorATan2(Src0, Src1);
	}
};

struct FVectorKernelCeil : public TUnaryVectorKernel<FVectorKernelCeil>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorCeil(Src0);
	}
};

struct FVectorKernelFloor : public TUnaryVectorKernel<FVectorKernelFloor>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorFloor(Src0);
	}
};

struct FVectorKernelRound : public TUnaryVectorKernel<FVectorKernelRound>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		//TODO: >SSE4 has direct ops for this.		
		VectorRegister4Float Trunc = VectorTruncate(Src0);
		*Dst = VectorAdd(Trunc, VectorTruncate(VectorMultiply(VectorSubtract(Src0, Trunc), GlobalVectorConstants::FloatAlmostTwo())));
	}
};

struct FVectorKernelMod : public TBinaryVectorKernel<FVectorKernelMod>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorMod(Src0, Src1);
	}
};

struct FVectorKernelFrac : public TUnaryVectorKernel<FVectorKernelFrac>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorFractional(Src0);
	}
};

struct FVectorKernelTrunc : public TUnaryVectorKernel<FVectorKernelTrunc>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorTruncate(Src0);
	}
};

struct FVectorKernelCompareLT : public TBinaryVectorKernel<FVectorKernelCompareLT>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorCompareLT(Src0, Src1);
	}
};

struct FVectorKernelCompareLE : public TBinaryVectorKernel<FVectorKernelCompareLE>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorCompareLE(Src0, Src1);
	}
};

struct FVectorKernelCompareGT : public TBinaryVectorKernel<FVectorKernelCompareGT>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorCompareGT(Src0, Src1);
	}
};

struct FVectorKernelCompareGE : public TBinaryVectorKernel<FVectorKernelCompareGE>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorCompareGE(Src0, Src1);
	}
};

struct FVectorKernelCompareEQ : public TBinaryVectorKernel<FVectorKernelCompareEQ>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		*Dst = VectorCompareEQ(Src0, Src1);
	}
};

struct FVectorKernelCompareNEQ : public TBinaryVectorKernel<FVectorKernelCompareNEQ>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{		
		*Dst = VectorCompareNE(Src0, Src1);
	}
};

struct FVectorKernelSelect : public TTrinaryVectorKernel<FVectorKernelSelect>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Mask, VectorRegister4Float A, VectorRegister4Float B)
	{
		*Dst = VectorSelect(Mask, A, B);
	}
};

struct FVectorKernelExecutionIndex
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		FRegisterHandler<VectorRegister4Int>::Optimize(Context);
	}

	static void VM_FORCEINLINE Exec(FVectorVMContext& Context)
	{
		static_assert(VECTOR_WIDTH_FLOATS == 4, "Need to update this when upgrading the VM to support >SSE2");
		VectorRegister4Int VectorStride = MakeVectorRegisterInt(VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS);
		VectorRegister4Int Index = MakeVectorRegisterInt(Context.StartInstance, Context.StartInstance + 1, Context.StartInstance + 2, Context.StartInstance + 3);
		
		FRegisterHandler<VectorRegister4Int> Dest(Context);
		const int32 Loops = Context.GetNumLoops<VECTOR_WIDTH_FLOATS>();
		for (int32 i = 0; i < Loops; ++i)
		{
			*Dest.GetDestAndAdvance() = Index;
			Index = VectorIntAdd(Index, VectorStride);
		}
	}
};

struct FVectorKernelEnterStatScope
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
#if STATS
		Context.WriteExecFunction(Exec);
		FConstantHandler<int32>::Optimize(Context);
#elif ENABLE_STATNAMEDEVENTS
		if ( GbDetailedVMScriptStats )
		{
			Context.WriteExecFunction(Exec);
			FConstantHandler<int32>::Optimize(Context);
		}
		else
		{
			FConstantHandler<int32>::OptimizeSkip(Context);
		}
#else
		// just skip the op if we don't have stats enabled
		FConstantHandler<int32>::OptimizeSkip(Context);
#endif
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		FConstantHandler<int32> ScopeIdx(Context);
#if STATS
		if (GbDetailedVMScriptStats && Context.StatScopes.Num())
		{
			int32 CounterIdx = Context.StatCounterStack.AddDefaulted(1);
			int32 ScopeIndex = ScopeIdx.Get();
			Context.StatCounterStack[CounterIdx].CycleCounter.Start(Context.StatScopes[ScopeIndex].StatId);
			Context.StatCounterStack[CounterIdx].VmCycleCounter = { ScopeIndex, FPlatformTime::Cycles64() };
		}
#elif ENABLE_STATNAMEDEVENTS
		if (Context.StatNamedEventScopes.Num())
		{
			FPlatformMisc::BeginNamedEvent(FColor::Red, *Context.StatNamedEventScopes[ScopeIdx.Get()]);
		}
#endif
	}
};

struct FVectorKernelExitStatScope
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
#if STATS
		Context.WriteExecFunction(Exec);
#elif ENABLE_STATNAMEDEVENTS
		if (GbDetailedVMScriptStats)
		{
			Context.WriteExecFunction(Exec);
		}
#endif
	}
		
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
#if STATS
		if (GbDetailedVMScriptStats && Context.StatScopes.Num())
		{
			FStatStackEntry& StackEntry = Context.StatCounterStack.Last();
			StackEntry.CycleCounter.Stop();
			Context.ScopeExecCycles[StackEntry.VmCycleCounter.ScopeIndex] += FPlatformTime::Cycles64() - StackEntry.VmCycleCounter.ScopeEnterCycles;
			Context.StatCounterStack.Pop(EAllowShrinking::No);
		}
#elif ENABLE_STATNAMEDEVENTS
		if (Context.StatNamedEventScopes.Num())
		{
			FPlatformMisc::EndNamedEvent();
		}
#endif
	}
};

struct FVectorKernelRandom : public TUnaryVectorKernel<FVectorKernelRandom>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		//EEK!. Improve this. Implement GPU style seeded rand instead of this.
		VectorRegister4Float Result = MakeVectorRegister(Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction());
		*Dst = VectorMultiply(Result, Src0);
	}
};

/* gaussian distribution random number (not working yet) */
struct FVectorKernelRandomGauss : public TBinaryVectorKernel<FVectorKernelRandomGauss>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		VectorRegister4Float Result = MakeVectorRegister(Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction(),
			Context.RandStream.GetFraction());

		Result = VectorSubtract(Result, GlobalVectorConstants::FloatOneHalf);
		Result = VectorMultiply(MakeVectorRegister(3.0f, 3.0f, 3.0f, 3.0f), Result);

		// taylor series gaussian approximation
		const VectorRegister4Float SPi2 = VectorReciprocal(VectorReciprocalSqrt(MakeVectorRegister(2 * PI, 2 * PI, 2 * PI, 2 * PI)));
		VectorRegister4Float Gauss = VectorReciprocal(SPi2);
		VectorRegister4Float Div = VectorMultiply(GlobalVectorConstants::FloatTwo, SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorMultiply(Result, Result), Div));
		Div = VectorMultiply(MakeVectorRegister(8.0f, 8.0f, 8.0f, 8.0f), SPi2);
		Gauss = VectorAdd(Gauss, VectorDivide(VectorPow(MakeVectorRegister(4.0f, 4.0f, 4.0f, 4.0f), Result), Div));
		Div = VectorMultiply(MakeVectorRegister(48.0f, 48.0f, 48.0f, 48.0f), SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorPow(MakeVectorRegister(6.0f, 6.0f, 6.0f, 6.0f), Result), Div));

		Gauss = VectorDivide(Gauss, MakeVectorRegister(0.4f, 0.4f, 0.4f, 0.4f));
		Gauss = VectorMultiply(Gauss, Src0);
		*Dst = Gauss;
	}
};

struct FVectorKernelMin : public TBinaryVectorKernel<FVectorKernelMin>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorMin(Src0, Src1);
	}
};

struct FVectorKernelMax : public TBinaryVectorKernel<FVectorKernelMax>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorMax(Src0, Src1);
	}
};

struct FVectorKernelPow : public TBinaryVectorKernel<FVectorKernelPow>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst,VectorRegister4Float Src0,VectorRegister4Float Src1)
	{
		*Dst = VectorPow(Src0, Src1);
	}
};

// if the base is small, then the result will be 0
struct FVectorKernelPowSafe : public TBinaryVectorKernel<FVectorKernelPowSafe>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0, VectorRegister4Float Src1)
	{
		VectorRegister4Float ValidMask = VectorCompareGT(Src0, GlobalVectorConstants::SmallNumber);
		*Dst = VectorSelect(ValidMask, VectorPow(Src0, Src1), GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelSign : public TUnaryVectorKernel<FVectorKernelSign>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorSign(Src0);
	}
};

namespace VectorVMNoise
{
	int32 P[512] =
	{
		151,160,137,91,90,15,
		131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
		190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
		88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
		77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
		102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
		135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
		5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
		223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
		129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
		251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
		49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
		138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
		151,160,137,91,90,15,
		131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
		190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
		88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
		77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
		102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
		135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
		5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
		223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
		129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
		251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
		49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
		138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
	};

	static FORCEINLINE float Lerp(float X, float A, float B)
	{
		return A + X * (B - A);
	}

	static FORCEINLINE float Fade(float X)
	{
		return X * X * X * (X * (X * 6 - 15) + 10);
	}
	
	static FORCEINLINE float Grad(int32 hash, float x, float y, float z)
	{
		 hash &= 15;
		 float u = (hash < 8) ? x : y;
		 float v = (hash < 4) ? y : ((hash == 12 || hash == 14) ? x : z);
		 return ((hash & 1) == 0 ? u : -u) + ((hash & 2) == 0 ? v : -v);
	}

	struct FScalarKernelNoise3D_iNoise : TTrinaryScalarKernel<FScalarKernelNoise3D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, float* RESTRICT Dst, float X, float Y, float Z)
		{
			float Xfl = FMath::FloorToFloat(X);
			float Yfl = FMath::FloorToFloat(Y);
			float Zfl = FMath::FloorToFloat(Z);
			int32 Xi = (int32)(Xfl) & 255;
			int32 Yi = (int32)(Yfl) & 255;
			int32 Zi = (int32)(Zfl) & 255;
			X -= Xfl;
			Y -= Yfl;
			Z -= Zfl;
			float Xm1 = X - 1.0f;
			float Ym1 = Y - 1.0f;
			float Zm1 = Z - 1.0f;

			int32 A = P[Xi] + Yi;
			int32 AA = P[A] + Zi;	int32 AB = P[A + 1] + Zi;

			int32 B = P[Xi + 1] + Yi;
			int32 BA = P[B] + Zi;	int32 BB = P[B + 1] + Zi;

			float U = Fade(X);
			float V = Fade(Y);
			float W = Fade(Z);

			*Dst =
				Lerp(W,
					Lerp(V,
						Lerp(U,
							Grad(P[AA], X, Y, Z),
							Grad(P[BA], Xm1, Y, Z)),
						Lerp(U,
							Grad(P[AB], X, Ym1, Z),
							Grad(P[BB], Xm1, Ym1, Z))),
					Lerp(V,
						Lerp(U,
							Grad(P[AA + 1], X, Y, Zm1),
							Grad(P[BA + 1], Xm1, Y, Zm1)),
						Lerp(U,
							Grad(P[AB + 1], X, Ym1, Zm1),
							Grad(P[BB + 1], Xm1, Ym1, Zm1))));
		}
	};

	struct FScalarKernelNoise2D_iNoise : TBinaryScalarKernel<FScalarKernelNoise2D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, float* RESTRICT Dst, float X, float Y)
		{
			*Dst = 0.0f;//TODO
		}
	};

	struct FScalarKernelNoise1D_iNoise : TUnaryScalarKernel<FScalarKernelNoise1D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, float* RESTRICT Dst, float X)
		{
			*Dst = 0.0f;//TODO;
		}
	};

	static void Noise1D(FVectorVMContext& Context) { FScalarKernelNoise1D_iNoise::Exec(Context); }
	static void Noise2D(FVectorVMContext& Context) { FScalarKernelNoise2D_iNoise::Exec(Context); }
	static void Noise3D(FVectorVMContext& Context)
	{
		//Basic scalar implementation of perlin's improved noise until I can spend some quality time exploring vectorized implementations of Marc O's noise from Random.ush.
		//http://mrl.nyu.edu/~perlin/noise/
		FScalarKernelNoise3D_iNoise::Exec(Context);
	}

	static void Optimize_Noise1D(FVectorVMCodeOptimizerContext& Context) { FScalarKernelNoise1D_iNoise::Optimize(Context); }
	static void Optimize_Noise2D(FVectorVMCodeOptimizerContext& Context) { FScalarKernelNoise2D_iNoise::Optimize(Context); }
	static void Optimize_Noise3D(FVectorVMCodeOptimizerContext& Context) { FScalarKernelNoise3D_iNoise::Optimize(Context); }
};

//Olaf's orginal curl noise. Needs updating for the new scalar VM and possibly calling Curl Noise to avoid confusion with regular noise?
//Possibly needs to be a data interface as the VM can't output Vectors?
struct FVectorKernelNoise : public TUnaryVectorKernel<FVectorKernelNoise>
{
	static VectorRegister4Float RandomTable[17][17][17];

	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* RESTRICT Dst, VectorRegister4Float Src0)
	{
		const VectorRegister4Float VecSize = MakeVectorRegister(16.0f, 16.0f, 16.0f, 16.0f);

		*Dst = GlobalVectorConstants::FloatZero;
		
		for (uint32 i = 1; i < 2; i++)
		{
			float Di = 0.2f * (1.0f/(1<<i));
			VectorRegister4Float Div = MakeVectorRegister(Di, Di, Di, Di);
			VectorRegister4Float Coords = VectorMod( VectorAbs( VectorMultiply(Src0, Div) ), VecSize );
			const float *CoordPtr = reinterpret_cast<float const*>(&Coords);
			const int32 Cx = CoordPtr[0];
			const int32 Cy = CoordPtr[1];
			const int32 Cz = CoordPtr[2];

			VectorRegister4Float Frac = VectorFractional(Coords);
			VectorRegister4Float Alpha = VectorReplicate(Frac, 0);
			VectorRegister4Float OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Alpha);
			
			VectorRegister4Float XV1 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz], OneMinusAlpha));
			VectorRegister4Float XV2 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz], OneMinusAlpha));
			VectorRegister4Float XV3 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz+1], OneMinusAlpha));
			VectorRegister4Float XV4 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz+1], OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 1);
			OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Alpha);
			VectorRegister4Float YV1 = VectorMultiplyAdd(XV1, Alpha, VectorMultiply(XV2, OneMinusAlpha));
			VectorRegister4Float YV2 = VectorMultiplyAdd(XV3, Alpha, VectorMultiply(XV4, OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 2);
			OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Alpha);
			VectorRegister4Float ZV = VectorMultiplyAdd(YV1, Alpha, VectorMultiply(YV2, OneMinusAlpha));

			*Dst = VectorAdd(*Dst, ZV);
		}
	}
};

namespace VectorKernelNoiseImpl
{
	static void BuildNoiseTable()
	{
		// random noise
		float TempTable[17][17][17];
		for (int z = 0; z < 17; z++)
		{
			for (int y = 0; y < 17; y++)
			{
				for (int x = 0; x < 17; x++)
				{
					float f1 = (float)FMath::FRandRange(-1.0f, 1.0f);
					TempTable[x][y][z] = f1;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable[i][j][16] = TempTable[i][j][0];
				TempTable[i][16][j] = TempTable[i][0][j];
				TempTable[16][j][i] = TempTable[0][j][i];
			}
		}

		// compute gradients
		FVector3f TempTable2[17][17][17];
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector3f XGrad = FVector3f(1.0f, 0.0f, TempTable[x][y][z] - TempTable[x + 1][y][z]);
					FVector3f YGrad = FVector3f(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y + 1][z]);
					FVector3f ZGrad = FVector3f(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y][z + 1]);

					FVector3f Grad = FVector3f(XGrad.Z, YGrad.Z, ZGrad.Z);
					TempTable2[x][y][z] = Grad;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable2[i][j][16] = TempTable2[i][j][0];
				TempTable2[i][16][j] = TempTable2[i][0][j];
				TempTable2[16][j][i] = TempTable2[0][j][i];
			}
		}


		// compute curl of gradient field
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector3f Dy = TempTable2[x][y][z] - TempTable2[x][y + 1][z];
					FVector3f Sy = TempTable2[x][y][z] + TempTable2[x][y + 1][z];
					FVector3f Dx = TempTable2[x][y][z] - TempTable2[x + 1][y][z];
					FVector3f Sx = TempTable2[x][y][z] + TempTable2[x + 1][y][z];
					FVector3f Dz = TempTable2[x][y][z] - TempTable2[x][y][z + 1];
					FVector3f Sz = TempTable2[x][y][z] + TempTable2[x][y][z + 1];
					FVector3f Dir = FVector3f(Dy.Z - Sz.Y, Dz.X - Sx.Z, Dx.Y - Sy.X);

					FVectorKernelNoise::RandomTable[x][y][z] = MakeVectorRegister(Dir.X, Dir.Y, Dir.Z, 0.f);
				}
			}
		}
	}
}

VectorRegister4Float FVectorKernelNoise::RandomTable[17][17][17];

//////////////////////////////////////////////////////////////////////////
//Special Kernels.

/** Special kernel for acquiring a new ID. TODO. Can be written as general RWBuffer ops when we support that. */
struct FScalarKernelAcquireID
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());		// DataSetIndex
		Context.Write(Context.DecodeU16());		// IDIndexReg
		Context.Write(Context.DecodeU16());		// IDTagReg
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();
		const TArrayView<FDataSetMeta> MetaTable = Context.DataSetMetaTable;
		TArray<int32>&RESTRICT FreeIDTable = *MetaTable[DataSetIndex].FreeIDTable;
		TArray<int32>&RESTRICT SpawnedIDsTable = *MetaTable[DataSetIndex].SpawnedIDsTable;

		const int32 Tag = MetaTable[DataSetIndex].IDAcquireTag;

		const int32 IDIndexReg = Context.DecodeU16();
		int32*RESTRICT IDIndex = (int32*)(Context.GetTempRegister(IDIndexReg));

		const int32 IDTagReg = Context.DecodeU16();
		int32*RESTRICT IDTag = (int32*)(Context.GetTempRegister(IDTagReg));

		int32& NumFreeIDs = *MetaTable[DataSetIndex].NumFreeIDs;

		//Temporarily using a lock to ensure thread safety for accessing the FreeIDTable until a lock free solution can be implemented.
		MetaTable[DataSetIndex].LockFreeTable();
	
		check(FreeIDTable.Num() >= Context.NumInstances);
		check(NumFreeIDs >= Context.NumInstances);
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 FreeIDTableIndex = --NumFreeIDs;

			//Grab the value from the FreeIDTable.
			int32 AcquiredID = FreeIDTable[FreeIDTableIndex];
			checkSlow(AcquiredID != INDEX_NONE);

			//UE_LOG(LogVectorVM, Warning, TEXT("AcquireID: ID:%d | FreeTableIdx:%d."), AcquiredID, FreeIDTableIndex);
			//Mark this entry in the FreeIDTable as invalid.
			FreeIDTable[FreeIDTableIndex] = INDEX_NONE;

			*IDIndex = AcquiredID;
			*IDTag = Tag;
			++IDIndex;
			++IDTag;

			SpawnedIDsTable.Add(AcquiredID);
		}

		MetaTable[DataSetIndex].UnlockFreeTable();
	}
};

/** Special kernel for updating a new ID. TODO. Can be written as general RWBuffer ops when we support that. */
struct FScalarKernelUpdateID
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());		// DataSetIndex
		Context.Write(Context.DecodeU16());		// InstanceIDRegisterIndex
		Context.Write(Context.DecodeU16());		// InstanceIndexRegisterIndex
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();
		const int32 InstanceIDRegisterIndex = Context.DecodeU16();
		const int32 InstanceIndexRegisterIndex = Context.DecodeU16();

		const TArrayView<FDataSetMeta> MetaTable = Context.DataSetMetaTable;

		TArray<int32>&RESTRICT IDTable = *MetaTable[DataSetIndex].IDTable;
		const int32 InstanceOffset = MetaTable[DataSetIndex].InstanceOffset;
		const int32 AbsoluteStartInstance = InstanceOffset + Context.StartInstance;

		const int32*RESTRICT IDRegister = (int32*)(Context.GetTempRegister(InstanceIDRegisterIndex));
		const int32*RESTRICT IndexRegister = (int32*)(Context.GetTempRegister(InstanceIndexRegisterIndex));
		
		FDataSetThreadLocalTempData& DataSetTempData = Context.ThreadLocalTempData[DataSetIndex];

		TArray<int32>&RESTRICT IDsToFree = DataSetTempData.IDsToFree;
		check(IDTable.Num() >= AbsoluteStartInstance + Context.NumInstances);
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 InstanceId = IDRegister[i];
			int32 Index = IndexRegister[i];

			if (Index == INDEX_NONE)
			{
				//Add the ID to a thread local list of IDs to free which are actually added to the list safely at the end of this chunk's execution.
				IDsToFree.Add(InstanceId);
				//UE_LOG(LogVectorVM, Warning, TEXT("FreeingID: InstanceID:%d."), InstanceId);
			}
			else
			{
				// Update the actual index for this ID. No thread safety is needed as this ID slot can only ever be written by this instance and so a single thread.
				// The index passed into this function is the same as that given to the OutputData*() functions (FScalarKernelWriteOutputIndexed kernel).
				// That value is local to the execution step (update or spawn), so we must offset it by the start instance number for the step, just like
				// GetOutputRegister() does.
				IDTable[InstanceId] = Index + InstanceOffset;

				//Update thread local max ID seen. We push this to the real value at the end of execution.
				DataSetTempData.MaxID = FMath::Max(DataSetTempData.MaxID, InstanceId);
				
				//UE_LOG(LogVectorVM, Warning, TEXT("UpdateID: RealIdx:%d | InstanceID:%d."), RealIdx, InstanceId);
			}
		}
	}
};

/** Special kernel for reading from the main input dataset. */
template<typename SourceType, int TypeOffset>
struct FVectorKernelReadInput
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());	// DataSetIndex
		Context.Write(Context.DecodeU16());	// InputRegisterIdx
		Context.Write(Context.DecodeU16());	// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		static const int32 InstancesPerVector = sizeof(VectorRegister4Float ) / sizeof(SourceType);

		const int32 DataSetIndex = Context.DecodeU16();
		const int32 InputRegisterIdx = Context.DecodeU16();
		const int32 DestRegisterIdx = Context.DecodeU16();
		int32 Loops = Context.GetNumLoops<InstancesPerVector>();

		VectorRegister4Float* DestReg = (VectorRegister4Float*)(Context.GetTempRegister(DestRegisterIdx));
		VectorRegister4Float* InputReg = (VectorRegister4Float*)(Context.GetInputRegister<SourceType, TypeOffset>(DataSetIndex, InputRegisterIdx) + Context.GetStartInstance());

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		while(Loops > 0)
		{
			*DestReg = VectorLoad(InputReg);
			++DestReg;
			++InputReg;
			Loops--;
		}
	}
};

template<>
struct FVectorKernelReadInput<FFloat16, 2>
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());	// DataSetIndex
		Context.Write(Context.DecodeU16());	// InputRegisterIdx
		Context.Write(Context.DecodeU16());	// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();
		const int32 InputRegisterIdx = Context.DecodeU16();
		const int32 DestRegisterIdx = Context.DecodeU16();
		int32 Loops = Context.GetNumLoops<4>();

		float* DestReg = (float*)(Context.GetTempRegister(DestRegisterIdx));
		uint16* InputReg = (uint16*)(Context.GetInputRegister<FFloat16, 2>(DataSetIndex, InputRegisterIdx) + Context.GetStartInstance());

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		while (Loops > 1)
		{
			FPlatformMath::WideVectorLoadHalf(DestReg, InputReg);
			DestReg += 8;
			InputReg += 8;
			Loops -= 2;
		}

		while (Loops > 0)
		{
			FPlatformMath::VectorLoadHalf(DestReg, InputReg);
			DestReg += 4;
			InputReg += 4;
			Loops -= 1;
		}
	}
};


/** Special kernel for reading from an input dataset; non-advancing (reads same instance everytime). 
 *  this kernel splats the X component of the source register to all 4 dest components; it's meant to
 *	use scalar data sets as the source (e.g. events)
 */
template<typename T, int TypeOffset>
struct FVectorKernelReadInputNoAdvance
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());	// DataSetIndex
		Context.Write(Context.DecodeU16());	// InputRegisterIdx
		Context.Write(Context.DecodeU16());	// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		static const int32 InstancesPerVector = sizeof(VectorRegister4Float) / sizeof(T);

		const int32 DataSetIndex = Context.DecodeU16();
		const int32 InputRegisterIdx = Context.DecodeU16();
		const int32 DestRegisterIdx = Context.DecodeU16();
		const int32 Loops = Context.GetNumLoops<InstancesPerVector>();

		VectorRegister4Float* DestReg = (VectorRegister4Float*)(Context.GetTempRegister(DestRegisterIdx));
		VectorRegister4Float* InputReg = (VectorRegister4Float*)(Context.GetInputRegister<T, TypeOffset>(DataSetIndex, InputRegisterIdx));

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		for (int32 i = 0; i < Loops; ++i)
		{
			*DestReg = VectorSwizzle(VectorLoad(InputReg), 0,0,0,0);
			++DestReg;
		}
	}
};

template<>
struct FVectorKernelReadInputNoAdvance<FFloat16, 2>
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.WriteExecFunction(Exec);
		Context.Write(Context.DecodeU16());	// DataSetIndex
		Context.Write(Context.DecodeU16());	// InputRegisterIdx
		Context.Write(Context.DecodeU16());	// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();
		const int32 InputRegisterIdx = Context.DecodeU16();
		const int32 DestRegisterIdx = Context.DecodeU16();
		const int32 Loops = Context.GetNumLoops<4>();

		VectorRegister4Float* DestReg = (VectorRegister4Float*)(Context.GetTempRegister(DestRegisterIdx));
		uint16* InputReg = (uint16*)(Context.GetInputRegister<FFloat16, 2>(DataSetIndex, InputRegisterIdx));

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		for (int32 i = 0; i < Loops; ++i)
		{
			float flt = FPlatformMath::LoadHalf(InputReg);
			*DestReg = VectorLoadFloat1(&flt);
			++DestReg;
		}
	}
};

//TODO - Should be straight forwards to follow the input with a mix of the outputs direct indexing
/** Special kernel for reading an specific location in an input register. */
// template<typename T>
// struct FScalarKernelReadInputIndexed
// {
// 	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
// 	{
// 		int32* IndexReg = (int32*)(Context.RegisterTable[DecodeU16(Context)]);
// 		T* InputReg = (T*)(Context.RegisterTable[DecodeU16(Context)]);
// 		T* DestReg = (T*)(Context.RegisterTable[DecodeU16(Context)]);
// 
// 		//Has to be scalar as each instance can read from a different location in the input buffer.
// 		for (int32 i = 0; i < Context.NumInstances; ++i)
// 		{
// 			T* ReadPtr = (*InputReg) + (*IndexReg);
// 			*DestReg = (*ReadPtr);
// 			++IndexReg;
// 			++DestReg;
// 		}
// 	}
// };

/** Special kernel for writing to a specific output register. */
template<typename SourceType, typename DestType, int TypeOffset>
struct FScalarKernelWriteOutputIndexed
{
	static VM_FORCEINLINE void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpTypes = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
			case SRCOP_RRR: Context.WriteExecFunction(DoKernel<FRegisterHandler<SourceType>>); break;
			case SRCOP_RRC:	Context.WriteExecFunction(DoKernel<FConstantHandler<SourceType>>); break;
			default: check(0); break;
		};

		Context.Write(Context.DecodeU16());		// DataSetIndex
		Context.Write(Context.DecodeU16());		// DestIndexRegisterIdx
		Context.Write(Context.DecodeU16());		// DataHandlerType
		Context.Write(Context.DecodeU16());		// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const uint32 SrcOpTypes = Context.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: DoKernel<FRegisterHandler<SourceType>>(Context); break;
		case SRCOP_RRC:	DoKernel<FConstantHandler<SourceType>>(Context); break;
		default: check(0); break;
		};
	}

	template<typename DataHandlerType>
	static VM_FORCEINLINE void DoKernel(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();

		const int32 DestIndexRegisterIdx = Context.DecodeU16();
		int32* RESTRICT DestIndexReg = (int32*)(Context.GetTempRegister(DestIndexRegisterIdx));

		DataHandlerType DataHandler(Context);

		const int32 DestRegisterIdx = Context.DecodeU16();
		DestType* RESTRICT DestReg = Context.GetOutputRegister<DestType, TypeOffset>(DataSetIndex, DestRegisterIdx);

		int NumInstances = Context.GetNumInstances();
		for (int32 i = 0; i < NumInstances; ++i)
		{
			int32 DestIndex = *DestIndexReg;
			if (DestIndex != INDEX_NONE)
			{
				DestReg[DestIndex] = DataHandler.Get();
			}

			++DestIndexReg;
			DataHandler.Advance();
			//We don't increment the dest as we index into it directly.
		}
	}
};

template<>
struct FScalarKernelWriteOutputIndexed<float, FFloat16, 2>
{
	static VM_FORCEINLINE void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpTypes = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: Context.WriteExecFunction(DoKernel<FRegisterHandler<float>>); break;
		case SRCOP_RRC:	Context.WriteExecFunction(DoKernel<FConstantHandler<float>>); break;
		default: check(0); break;
		};

		Context.Write(Context.DecodeU16());		// DataSetIndex
		Context.Write(Context.DecodeU16());		// DestIndexRegisterIdx
		Context.Write(Context.DecodeU16());		// DataHandlerType
		Context.Write(Context.DecodeU16());		// DestRegisterIdx
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		const uint32 SrcOpTypes = Context.DecodeSrcOperandTypes();
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: DoKernel<FRegisterHandler<float>>(Context); break;
		case SRCOP_RRC:	DoKernel<FConstantHandler<float>>(Context); break;
		default: check(0); break;
		};
	}

	template<typename DataHandlerType>
	static VM_FORCEINLINE void DoKernel(FVectorVMContext& Context)
	{
		const int32 DataSetIndex = Context.DecodeU16();

		const int32 DestIndexRegisterIdx = Context.DecodeU16();
		int32* RESTRICT DestIndexReg = (int32*)(Context.GetTempRegister(DestIndexRegisterIdx));

		DataHandlerType DataHandler(Context);

		const int32 DestRegisterIdx = Context.DecodeU16();
		uint16* RESTRICT DestReg = (uint16*)Context.GetOutputRegister<FFloat16, 2>(DataSetIndex, DestRegisterIdx);

		int NumInstances = Context.GetNumInstances();
		for (int32 i = 0; i < NumInstances; ++i)
		{
			int32 DestIndex = *DestIndexReg;
			if (DestIndex != INDEX_NONE)
			{
				FPlatformMath::StoreHalf(&DestReg[DestIndex], DataHandler.Get());
			}

			++DestIndexReg;
			DataHandler.Advance();
			//We don't increment the dest as we index into it directly.
		}
	}
};

struct FDataSetCounterHandler
{
	int32* Counter;
	FDataSetCounterHandler(FVectorVMContext& Context)
		: Counter(&Context.GetDataSetMeta(Context.DecodeU16()).DataSetAccessIndex)
	{}

	VM_FORCEINLINE void Advance() { }
	VM_FORCEINLINE int32* Get() { return Counter; }
	VM_FORCEINLINE int32* GetAndAdvance() { return Counter; }
	//VM_FORCEINLINE const int32* GetDest() { return Counter; }Should never use as a dest. All kernels with read and write to this.

	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		Context.Write(Context.DecodeU16());
	}
};

struct FScalarKernelAcquireCounterIndex
{
	template<bool bThreadsafe>
	struct InternalKernel
	{
		static VM_FORCEINLINE void DoKernel(FVectorVMContext& Context, int32* RESTRICT Dst, int32* Index, int32 Valid)
		{
			if (Valid != 0)
			{
				*Dst = bThreadsafe ? FPlatformAtomics::InterlockedIncrement(Index) : ++(*Index);
			}
			else
			{
				*Dst = INDEX_NONE;	// Subsequent DoKernal calls above will skip over INDEX_NONE register entries...
			}
		}

		static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
		{
			const uint32 SrcOpType = Context.DecodeSrcOperandTypes();
			switch (SrcOpType)
			{
				case SRCOP_RRR: TBinaryKernelHandler<InternalKernel<true>, FRegisterHandler<int32>, FDataSetCounterHandler, FRegisterHandler<int32>, 1>::Exec(Context); break;
				case SRCOP_RRC:	TBinaryKernelHandler<InternalKernel<true>, FRegisterHandler<int32>, FDataSetCounterHandler, FConstantHandler<int32>, 1>::Exec(Context); break;
				default: check(0); break;
			};
		}
	};

	template<uint32 SrcOpType>
	static void ExecOptimized(FVectorVMContext& Context)
	{
		if (Context.IsParallelExecution())
		{
			switch (SrcOpType)
			{
				case SRCOP_RRR: TBinaryKernelHandler<InternalKernel<true>, FRegisterHandler<int32>, FDataSetCounterHandler, FRegisterHandler<int32>, 1>::Exec(Context); break;
				case SRCOP_RRC:	TBinaryKernelHandler<InternalKernel<true>, FRegisterHandler<int32>, FDataSetCounterHandler, FConstantHandler<int32>, 1>::Exec(Context); break;
				default: check(0); break;
			}
		}
		else
		{
			switch (SrcOpType)
			{
				case SRCOP_RRR: TBinaryKernelHandler<InternalKernel<false>, FRegisterHandler<int32>, FDataSetCounterHandler, FRegisterHandler<int32>, 1>::Exec(Context); break;
				case SRCOP_RRC:	TBinaryKernelHandler<InternalKernel<false>, FRegisterHandler<int32>, FDataSetCounterHandler, FConstantHandler<int32>, 1>::Exec(Context); break;
				default: check(0); break;
			}
		}
	}

	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpType = Context.BaseContext.DecodeSrcOperandTypes();
		switch (SrcOpType)
		{
			case SRCOP_RRR: Context.WriteExecFunction(FScalarKernelAcquireCounterIndex::ExecOptimized<SRCOP_RRR>); break;
			case SRCOP_RRC: Context.WriteExecFunction(FScalarKernelAcquireCounterIndex::ExecOptimized<SRCOP_RRC>); break;
			default: check(0); break;
		}

		// Three registers, note we don't call Optimize on the Kernel since that will write the Exec and we are selecting based upon thread safe or not
		Context.Write(Context.DecodeU16());
		Context.Write(Context.DecodeU16());
		Context.Write(Context.DecodeU16());
	}

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		if ( Context.IsParallelExecution() )
		{
			InternalKernel<true>::Exec(Context);
		}
		else
		{
			InternalKernel<false>::Exec(Context);
		}
	}

};

//TODO: REWORK TO FUNCITON LIKE THE ABOVE.
// /** Special kernel for decrementing a dataset counter. */
// struct FScalarKernelReleaseCounterIndex
// {
// 	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
// 	{
// 		int32* CounterPtr = (int32*)(Context.ConstantTable[DecodeU16(Context)]);
// 		int32* DestReg = (int32*)(Context.RegisterTable[DecodeU16(Context)]);
// 
// 		for (int32 i = 0; i < Context.NumInstances; ++i)
// 		{
// 			int32 Counter = (*CounterPtr--);
// 			*DestReg = Counter >= 0 ? Counter : INDEX_NONE;
// 
// 			++DestReg;
// 		}
// 	}
// };

//////////////////////////////////////////////////////////////////////////
//external_func_call

struct FKernelExternalFunctionCall
{
	static void Optimize(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 ExternalFuncIdx = Context.DecodeU8();

		Context.WriteExecFunction(Exec);
		Context.Write<uint8>(ExternalFuncIdx);

		const int32 NumRegisters = Context.ExternalFunctionRegisterCounts[ExternalFuncIdx];
		for ( int32 i=0; i < NumRegisters; ++i )
		{
			Context.Write(Context.DecodeU16());
		}
	}

	static void Exec(FVectorVMContext& Context)
	{
#if VECTORVM_SUPPORTS_LEGACY
		const uint32 ExternalFuncIdx = Context.DecodeU8();
		const FVMExternalFunction* ExternalFunction = Context.ExternalFunctionTable[ExternalFuncIdx];
		check(ExternalFunction);

		if (ExternalFunction)
		{
#if VECTORVM_SUPPORTS_EXPERIMENTAL
			FVectorVMExternalFunctionContext DataInterfaceFunctionContext(&Context);
#else
			FVectorVMExternalFunctionContextLegacy DataInterfaceFunctionContext(&Context);
#endif
			ExternalFunction->Execute(DataInterfaceFunctionContext);
		}
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
//Integer operations

//addi,
struct FVectorIntKernelAdd : TBinaryVectorIntKernel<FVectorIntKernelAdd>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntAdd(Src0, Src1);
	}
};

//subi,
struct FVectorIntKernelSubtract : TBinaryVectorIntKernel<FVectorIntKernelSubtract>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntSubtract(Src0, Src1);
	}
};

//muli,
struct FVectorIntKernelMultiply : TBinaryVectorIntKernel<FVectorIntKernelMultiply>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntMultiply(Src0, Src1);
	}
};

//divi,
struct FVectorIntKernelDivide : TBinaryVectorIntKernel<FVectorIntKernelDivide>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		int32 TmpA[4];
		VectorIntStore(Src0, TmpA);

		int32 TmpB[4];
		VectorIntStore(Src1, TmpB);

		// No intrinsics exist for integer divide. Since div by zero causes crashes, we must be safe against that.
		int32 TmpDst[4];
		TmpDst[0] = SafeIntDivide(TmpA[0], TmpB[0]);
		TmpDst[1] = SafeIntDivide(TmpA[1], TmpB[1]);
		TmpDst[2] = SafeIntDivide(TmpA[2], TmpB[2]);
		TmpDst[3] = SafeIntDivide(TmpA[3], TmpB[3]);

		*Dst = MakeVectorRegisterInt(TmpDst[0], TmpDst[1], TmpDst[2], TmpDst[3]);
	}

private:
	VM_FORCEINLINE static int32 SafeIntDivide(int32 Numerator, int32 Denominator)
	{
		static constexpr int32 MinIntValue = std::numeric_limits<int32>::min();
		static constexpr int32 MaxIntValue = std::numeric_limits<int32>::max();

		if (Denominator == 0)
		{
			return 0;
		}
		else if ((Denominator == -1) && (Numerator == MinIntValue))
		{
			return MaxIntValue;
		}

		return Numerator / Denominator;
	}
};


//clampi,
struct FVectorIntKernelClamp : TTrinaryVectorIntKernel<FVectorIntKernelClamp>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1, VectorRegister4Int Src2)
	{
		*Dst = VectorIntMin(VectorIntMax(Src0, Src1), Src2);
	}
};

//mini,
struct FVectorIntKernelMin : TBinaryVectorIntKernel<FVectorIntKernelMin>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntMin(Src0, Src1);
	}
};

//maxi,
struct FVectorIntKernelMax : TBinaryVectorIntKernel<FVectorIntKernelMax>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntMax(Src0, Src1);
	}
};

//absi,
struct FVectorIntKernelAbs : TUnaryVectorIntKernel<FVectorIntKernelAbs>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntAbs(Src0);
	}
};

//negi,
struct FVectorIntKernelNegate : TUnaryVectorIntKernel<FVectorIntKernelNegate>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntNegate(Src0);
	}
};

//signi,
struct FVectorIntKernelSign : TUnaryVectorIntKernel<FVectorIntKernelSign>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntSign(Src0);
	}
};

//randomi,
//No good way to do this with SSE atm so just do it scalar.
struct FScalarIntKernelRandom : public TUnaryScalarIntKernel<FScalarIntKernelRandom>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, int32* RESTRICT Dst, int32 Src0)
	{
		//EEK!. Improve this. Implement GPU style seeded rand instead of this.
		*Dst = static_cast<int32>(Context.RandStream.GetFraction() * Src0);
	}
};

//cmplti,
struct FVectorIntKernelCompareLT : TBinaryVectorIntKernel<FVectorIntKernelCompareLT>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareLT(Src0, Src1);
	}
};

//cmplei,
struct FVectorIntKernelCompareLE : TBinaryVectorIntKernel<FVectorIntKernelCompareLE>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareLE(Src0, Src1);
	}
};

//cmpgti,
struct FVectorIntKernelCompareGT : TBinaryVectorIntKernel<FVectorIntKernelCompareGT>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareGT(Src0, Src1);
	}
};

//cmpgei,
struct FVectorIntKernelCompareGE : TBinaryVectorIntKernel<FVectorIntKernelCompareGE>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareGE(Src0, Src1);
	}
};

//cmpeqi,
struct FVectorIntKernelCompareEQ : TBinaryVectorIntKernel<FVectorIntKernelCompareEQ>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareEQ(Src0, Src1);
	}
};

//cmpneqi,
struct FVectorIntKernelCompareNEQ : TBinaryVectorIntKernel<FVectorIntKernelCompareNEQ>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntCompareNEQ(Src0, Src1);
	}
};

//bit_and,
struct FVectorIntKernelBitAnd : TBinaryVectorIntKernel<FVectorIntKernelBitAnd>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntAnd(Src0, Src1);
	}
};

//bit_or,
struct FVectorIntKernelBitOr : TBinaryVectorIntKernel<FVectorIntKernelBitOr>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntOr(Src0, Src1);
	}
};

//bit_xor,
struct FVectorIntKernelBitXor : TBinaryVectorIntKernel<FVectorIntKernelBitXor>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		*Dst = VectorIntXor(Src0, Src1);
	}
};

//bit_not,
struct FVectorIntKernelBitNot : TUnaryVectorIntKernel<FVectorIntKernelBitNot>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntNot(Src0);
	}
};

// bit_lshift
struct FVectorIntKernelBitLShift : TBinaryVectorIntKernel<FVectorIntKernelBitLShift>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0,  VectorRegister4Int Src1)
	{
		int32 TmpA[4];
		VectorIntStore(Src0, TmpA);

		int32 TmpB[4];
		VectorIntStore(Src1, TmpB);

		int32 TmpDst[4];
		TmpDst[0] = (TmpA[0] << TmpB[0]);
		TmpDst[1] = (TmpA[1] << TmpB[1]);
		TmpDst[2] = (TmpA[2] << TmpB[2]);
		TmpDst[3] = (TmpA[3] << TmpB[3]);
		*Dst = MakeVectorRegisterInt(TmpDst[0], TmpDst[1], TmpDst[2], TmpDst[3] );
	}
};

// bit_rshift
struct FVectorIntKernelBitRShift : TBinaryVectorIntKernel<FVectorIntKernelBitRShift>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		int32 TmpA[4];
		VectorIntStore(Src0, TmpA);

		int32 TmpB[4];
		VectorIntStore(Src1, TmpB);

		int32 TmpDst[4];
		TmpDst[0] = (TmpA[0] >> TmpB[0]);
		TmpDst[1] = (TmpA[1] >> TmpB[1]);
		TmpDst[2] = (TmpA[2] >> TmpB[2]);
		TmpDst[3] = (TmpA[3] >> TmpB[3]);
		*Dst = MakeVectorRegisterInt(TmpDst[0], TmpDst[1], TmpDst[2], TmpDst[3]);
	}
};

//"Boolean" ops. Currently handling bools as integers.
//logic_and,
struct FVectorIntKernelLogicAnd : TBinaryVectorIntKernel<FVectorIntKernelLogicAnd>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntAnd(Src0, Src1);
	}
};

//logic_or,
struct FVectorIntKernelLogicOr : TBinaryVectorIntKernel<FVectorIntKernelLogicOr>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntOr(Src0, Src1);
	}
};
//logic_xor,
struct FVectorIntKernelLogicXor : TBinaryVectorIntKernel<FVectorIntKernelLogicXor>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0, VectorRegister4Int Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntXor(Src0, Src1);
	}
};

//logic_not,
struct FVectorIntKernelLogicNot : TUnaryVectorIntKernel<FVectorIntKernelLogicNot>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntNot(Src0);
	}
};

//conversions
//f2i,
struct FVectorKernelFloatToInt : TUnaryKernel<FVectorKernelFloatToInt, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorFloatToInt(Src0);
	}
};

//i2f,
struct FVectorKernelIntToFloat : TUnaryKernel<FVectorKernelIntToFloat, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntToFloat(Src0);
	}
};

//f2b,
struct FVectorKernelFloatToBool : TUnaryKernel<FVectorKernelFloatToBool, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* Dst, VectorRegister4Float Src0)
	{		
		*Dst = VectorCompareGT(Src0, GlobalVectorConstants::FloatZero);
	}
};

//b2f,
struct FVectorKernelBoolToFloat : TUnaryKernel<FVectorKernelBoolToFloat, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorSelect(Src0, GlobalVectorConstants::FloatOne, GlobalVectorConstants::FloatZero);
	}
};

//i2b,
struct FVectorKernelIntToBool : TUnaryKernel<FVectorKernelIntToBool, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntCompareGT(Src0, GlobalVectorConstants::IntZero);
	}
};

//b2i,
struct FVectorKernelBoolToInt : TUnaryKernel<FVectorKernelBoolToInt, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorIntSelect(Src0, GlobalVectorConstants::IntOne, GlobalVectorConstants::IntZero);
	}
};

//reinterpret bits
//fasi,
struct FVectorKernelFloatAsInt : TUnaryKernel<FVectorKernelFloatAsInt, FRegisterHandler<VectorRegister4Int>, FConstantHandler<VectorRegister4Float>, FRegisterHandler<VectorRegister4Float>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Int* Dst, VectorRegister4Float Src0)
	{
		*Dst = VectorCastFloatToInt(Src0);
	}
};

//iasf,
struct FVectorKernelIntAsFloat : TUnaryKernel<FVectorKernelIntAsFloat, FRegisterHandler<VectorRegister4Float>, FConstantHandler<VectorRegister4Int>, FRegisterHandler<VectorRegister4Int>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(FVectorVMContext& Context, VectorRegister4Float* Dst, VectorRegister4Int Src0)
	{
		*Dst = VectorCastIntToFloat(Src0);
	}
};

void VectorVM::Exec(FVectorVMExecArgs& Args, FVectorVMSerializeState *SerializeState)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(VMExec);
	SCOPE_CYCLE_COUNTER(STAT_VVMExec);

#if UE_BUILD_TEST
	const bool bNumInstancesEvent = GbDetailedVMScriptStats != 0;
	if (bNumInstancesEvent)
	{
		FPlatformMisc::BeginNamedEvent(FColor::Red, *FString::Printf(TEXT("STAT_VVMExec - %d"), Args.NumInstances));
	}
#endif

	const int32 MaxInstances = FMath::Min(GParallelVVMInstancesPerChunk, Args.NumInstances);
	const int32 NumChunks = (Args.NumInstances / GParallelVVMInstancesPerChunk) + 1;
	const int32 ChunksPerBatch = (GbParallelVVM != 0 && FApp::ShouldUseThreadingForPerformance()) ? GParallelVVMChunksPerBatch : NumChunks;
	const int32 NumBatches = FMath::DivideAndRoundUp(NumChunks, ChunksPerBatch);
	const bool bParallel = NumBatches > 1 && Args.bAllowParallel;
#	ifdef VVM_INCLUDE_SERIALIZATION
	const bool bUseOptimizedByteCode = false; //serializes the bytecode from the instructions, cannot use jump table
#	else //VVM_INCLUDE_SERIALIZATION
	const bool bUseOptimizedByteCode = (Args.OptimizedByteCode != nullptr) && GbUseOptimizedVMByteCode;
#	endif
	const FVectorVMExecFunction* OptimizedJumpTable = bUseOptimizedByteCode ? FVectorVMCodeOptimizerContext::DecodeJumpTable(Args.OptimizedByteCode) : nullptr;
	
	uint64 StartTime = FPlatformTime::Cycles64();

	auto ExecChunkBatch = [&](int32 BatchIdx)
	{
		//SCOPE_CYCLE_COUNTER(STAT_VVMExecChunk);

		FVectorVMContext& Context = FVectorVMContext::Get();
		Context.PrepareForExec(Args.NumTempRegisters, Args.ConstantTableCount, Args.ConstantTable, Args.ConstantTableSizes, Args.ExternalFunctionTable, Args.UserPtrTable, Args.DataSetMetaTable, MaxInstances, bParallel);
#if STATS
		Context.SetStatScopes(Args.StatScopes);
#elif ENABLE_STATNAMEDEVENTS
		Context.SetStatNamedEventScopes(Args.StatNamedEventsScopes);
#endif

		// Process one chunk at a time.
		int32 ChunkIdx = BatchIdx * ChunksPerBatch;
		const int32 FirstInstance = ChunkIdx * GParallelVVMInstancesPerChunk;
		const int32 FinalInstance = FMath::Min(Args.NumInstances, FirstInstance + (ChunksPerBatch * GParallelVVMInstancesPerChunk));
		int32 InstancesLeft = FinalInstance - FirstInstance;
		while (InstancesLeft > 0)
		{
			int32 NumInstancesThisChunk = FMath::Min(InstancesLeft, (int32)GParallelVVMInstancesPerChunk);
			int32 StartInstance = GParallelVVMInstancesPerChunk * ChunkIdx;

			// Execute optimized byte code version
			if ( bUseOptimizedByteCode )
			{
				// Setup execution context.
				Context.PrepareForChunk(Args.OptimizedByteCode, NumInstancesThisChunk, StartInstance);

				while (true)
				{
					FVectorVMExecFunction ExecFunction = OptimizedJumpTable[Context.DecodeU8()];
					if (ExecFunction == nullptr)
					{
						break;
					}
					ExecFunction(Context);
				}
			}
			else
			{
				// Setup execution context.
				Context.PrepareForChunk(Args.ByteCode, NumInstancesThisChunk, StartInstance);

				VVMSer_chunkStart(Context, ChunkIdx, BatchIdx);
				// Execute VM on all vectors in this chunk.
				EVectorVMOp Op = EVectorVMOp::done;
				do
				{
					VVMSer_insStart(Context);
					Op = Context.DecodeOp();
					switch (Op)
					{
						// Dispatch kernel ops.
						case EVectorVMOp::add: FVectorKernelAdd::Exec(Context); break;
						case EVectorVMOp::sub: FVectorKernelSub::Exec(Context); break;
						case EVectorVMOp::mul: FVectorKernelMul::Exec(Context); break;
						case EVectorVMOp::div: FVectorKernelDivSafe::Exec(Context); break;
						case EVectorVMOp::mad: FVectorKernelMad::Exec(Context); break;
						case EVectorVMOp::lerp: FVectorKernelLerp::Exec(Context); break;
						case EVectorVMOp::rcp: FVectorKernelRcpSafe::Exec(Context); break;
						case EVectorVMOp::rsq: FVectorKernelRsqSafe::Exec(Context); break;
						case EVectorVMOp::sqrt: FVectorKernelSqrtSafe::Exec(Context); break;
						case EVectorVMOp::neg: FVectorKernelNeg::Exec(Context); break;
						case EVectorVMOp::abs: FVectorKernelAbs::Exec(Context); break;
						case EVectorVMOp::exp: FVectorKernelExp::Exec(Context); break;
						case EVectorVMOp::exp2: FVectorKernelExp2::Exec(Context); break;
						case EVectorVMOp::log: FVectorKernelLogSafe::Exec(Context); break;
						case EVectorVMOp::log2: FVectorKernelLog2::Exec(Context); break;
						case EVectorVMOp::sin: FVectorKernelSin::Exec(Context); break;
						case EVectorVMOp::cos: FVectorKernelCos::Exec(Context); break;
						case EVectorVMOp::tan: FVectorKernelTan::Exec(Context); break;
						case EVectorVMOp::asin: FVectorKernelASin::Exec(Context); break;
						case EVectorVMOp::acos: FVectorKernelACos::Exec(Context); break;
						case EVectorVMOp::atan: FVectorKernelATan::Exec(Context); break;
						case EVectorVMOp::atan2: FVectorKernelATan2::Exec(Context); break;
						case EVectorVMOp::ceil: FVectorKernelCeil::Exec(Context); break;
						case EVectorVMOp::floor: FVectorKernelFloor::Exec(Context); break;
						case EVectorVMOp::round: FVectorKernelRound::Exec(Context); break;
						case EVectorVMOp::fmod: FVectorKernelMod::Exec(Context); break;
						case EVectorVMOp::frac: FVectorKernelFrac::Exec(Context); break;
						case EVectorVMOp::trunc: FVectorKernelTrunc::Exec(Context); break;
						case EVectorVMOp::clamp: FVectorKernelClamp::Exec(Context); break;
						case EVectorVMOp::min: FVectorKernelMin::Exec(Context); break;
						case EVectorVMOp::max: FVectorKernelMax::Exec(Context); break;
						case EVectorVMOp::pow: FVectorKernelPowSafe::Exec(Context); break;
						case EVectorVMOp::sign: FVectorKernelSign::Exec(Context); break;
						case EVectorVMOp::step: FVectorKernelStep::Exec(Context); break;
						case EVectorVMOp::random: FVectorKernelRandom::Exec(Context); break;
						case EVectorVMOp::noise: VectorVMNoise::Noise1D(Context); break;
						case EVectorVMOp::noise2D: VectorVMNoise::Noise2D(Context); break;
						case EVectorVMOp::noise3D: VectorVMNoise::Noise3D(Context); break;

						case EVectorVMOp::cmplt: FVectorKernelCompareLT::Exec(Context); break;
						case EVectorVMOp::cmple: FVectorKernelCompareLE::Exec(Context); break;
						case EVectorVMOp::cmpgt: FVectorKernelCompareGT::Exec(Context); break;
						case EVectorVMOp::cmpge: FVectorKernelCompareGE::Exec(Context); break;
						case EVectorVMOp::cmpeq: FVectorKernelCompareEQ::Exec(Context); break;
						case EVectorVMOp::cmpneq: FVectorKernelCompareNEQ::Exec(Context); break;
						case EVectorVMOp::select: FVectorKernelSelect::Exec(Context); break;

						case EVectorVMOp::addi: FVectorIntKernelAdd::Exec(Context); break;
						case EVectorVMOp::subi: FVectorIntKernelSubtract::Exec(Context); break;
						case EVectorVMOp::muli: FVectorIntKernelMultiply::Exec(Context); break;
						case EVectorVMOp::divi: FVectorIntKernelDivide::Exec(Context); break;
						case EVectorVMOp::clampi: FVectorIntKernelClamp::Exec(Context); break;
						case EVectorVMOp::mini: FVectorIntKernelMin::Exec(Context); break;
						case EVectorVMOp::maxi: FVectorIntKernelMax::Exec(Context); break;
						case EVectorVMOp::absi: FVectorIntKernelAbs::Exec(Context); break;
						case EVectorVMOp::negi: FVectorIntKernelNegate::Exec(Context); break;
						case EVectorVMOp::signi: FVectorIntKernelSign::Exec(Context); break;
						case EVectorVMOp::randomi: FScalarIntKernelRandom::Exec(Context); break;
						case EVectorVMOp::cmplti: FVectorIntKernelCompareLT::Exec(Context); break;
						case EVectorVMOp::cmplei: FVectorIntKernelCompareLE::Exec(Context); break;
						case EVectorVMOp::cmpgti: FVectorIntKernelCompareGT::Exec(Context); break;
						case EVectorVMOp::cmpgei: FVectorIntKernelCompareGE::Exec(Context); break;
						case EVectorVMOp::cmpeqi: FVectorIntKernelCompareEQ::Exec(Context); break;
						case EVectorVMOp::cmpneqi: FVectorIntKernelCompareNEQ::Exec(Context); break;
						case EVectorVMOp::bit_and: FVectorIntKernelBitAnd::Exec(Context); break;
						case EVectorVMOp::bit_or: FVectorIntKernelBitOr::Exec(Context); break;
						case EVectorVMOp::bit_xor: FVectorIntKernelBitXor::Exec(Context); break;
						case EVectorVMOp::bit_not: FVectorIntKernelBitNot::Exec(Context); break;
						case EVectorVMOp::bit_lshift: FVectorIntKernelBitLShift::Exec(Context); break;
						case EVectorVMOp::bit_rshift: FVectorIntKernelBitRShift::Exec(Context); break;
						case EVectorVMOp::logic_and: FVectorIntKernelLogicAnd::Exec(Context); break;
						case EVectorVMOp::logic_or: FVectorIntKernelLogicOr::Exec(Context); break;
						case EVectorVMOp::logic_xor: FVectorIntKernelLogicXor::Exec(Context); break;
						case EVectorVMOp::logic_not: FVectorIntKernelLogicNot::Exec(Context); break;
						case EVectorVMOp::f2i: FVectorKernelFloatToInt::Exec(Context); break;
						case EVectorVMOp::i2f: FVectorKernelIntToFloat::Exec(Context); break;
						case EVectorVMOp::f2b: FVectorKernelFloatToBool::Exec(Context); break;
						case EVectorVMOp::b2f: FVectorKernelBoolToFloat::Exec(Context); break;
						case EVectorVMOp::i2b: FVectorKernelIntToBool::Exec(Context); break;
						case EVectorVMOp::b2i: FVectorKernelBoolToInt::Exec(Context); break;
						case EVectorVMOp::fasi: FVectorKernelFloatAsInt::Exec(Context); break;
						case EVectorVMOp::iasf: FVectorKernelIntAsFloat::Exec(Context); break;

						case EVectorVMOp::outputdata_half:	FScalarKernelWriteOutputIndexed<float, FFloat16, 2>::Exec(Context);	break;
						case EVectorVMOp::inputdata_half: FVectorKernelReadInput<FFloat16, 2>::Exec(Context); break;
						case EVectorVMOp::outputdata_int32:	FScalarKernelWriteOutputIndexed<int32, int32, 1>::Exec(Context);	break;
						case EVectorVMOp::inputdata_int32: FVectorKernelReadInput<int32, 1>::Exec(Context); break;
						case EVectorVMOp::outputdata_float:	FScalarKernelWriteOutputIndexed<float, float, 0>::Exec(Context);	break;
						case EVectorVMOp::inputdata_float: FVectorKernelReadInput<float, 0>::Exec(Context); break;
						case EVectorVMOp::inputdata_noadvance_int32: FVectorKernelReadInputNoAdvance<int32, 1>::Exec(Context); break;
						case EVectorVMOp::inputdata_noadvance_float: FVectorKernelReadInputNoAdvance<float, 0>::Exec(Context); break;
						case EVectorVMOp::inputdata_noadvance_half: FVectorKernelReadInputNoAdvance<FFloat16, 2>::Exec(Context); break;
						case EVectorVMOp::acquireindex:	FScalarKernelAcquireCounterIndex::Exec(Context); break;
						case EVectorVMOp::external_func_call: FKernelExternalFunctionCall::Exec(Context); break;

						case EVectorVMOp::exec_index: FVectorKernelExecutionIndex::Exec(Context); break;

						case EVectorVMOp::enter_stat_scope: FVectorKernelEnterStatScope::Exec(Context); break;
						case EVectorVMOp::exit_stat_scope: FVectorKernelExitStatScope::Exec(Context); break;

						//Special case ops to handle unique IDs but this can be written as generalized buffer operations. TODO!
						case EVectorVMOp::update_id:	FScalarKernelUpdateID::Exec(Context); break;
						case EVectorVMOp::acquire_id:	FScalarKernelAcquireID::Exec(Context); break;

						// Execution always terminates with a "done" opcode.
						case EVectorVMOp::done:
							break;

						// Opcode not recognized / implemented.
						default:
							UE_LOG(LogVectorVM, Fatal, TEXT("Unknown op code 0x%02x"), (uint32)Op);
							return;//BAIL
					}
					VVMSer_insEnd(Context, (int)(VVMSerCtxStartInsCode - VVMSerStartCtxCode), (int)(Context.Code - VVMSerCtxStartInsCode));
				} while (Op != EVectorVMOp::done);
				VVMSer_chunkEnd(SerializeState)
			}

			InstancesLeft -= GParallelVVMInstancesPerChunk;
			++ChunkIdx;
		}
		Context.FinishExec();
	};

	if ( NumBatches > 1 )
	{
		ParallelFor(NumBatches, ExecChunkBatch, GbParallelVVM == 0 || !bParallel);
	}
	else
	{
		ExecChunkBatch(0);
	}

#if UE_BUILD_TEST
	if (bNumInstancesEvent)
	{
		FPlatformMisc::EndNamedEvent();
	}
#endif

#if VECTORVM_SUPPORTS_SERIALIZATION
	uint64 EndTime = FPlatformTime::Cycles64();
	if (SerializeState) {
		SerializeState->ExecDt = EndTime - StartTime; //NOTE: doesn't work if ParallelFor splits the work into multiple threads
	}
#endif //VVM_INCLUDE_SERIALIZATION
}

void ExecBatchedOutput(FVectorVMContext& Context)
{
	while (true)
	{
		FVectorVMExecFunction ExecFunction = reinterpret_cast<FVectorVMExecFunction>(Context.DecodePtr());
		if (ExecFunction == nullptr)
		{
			break;
		}
		ExecFunction(Context);
	}
}

void ConditionalAddOutputWrapper(FVectorVMCodeOptimizerContext& Context, bool& OuterAdded)
{
	if (!OuterAdded)
	{
		Context.WriteExecFunction(ExecBatchedOutput);
		OuterAdded = true;
	}
}

EVectorVMOp BatchedOutputOptimization(EVectorVMOp Op, FVectorVMCodeOptimizerContext& Context)
{
	if (!GbBatchVMOutput)
	{
		return Op;
	}

	bool OuterAdded = false;
	bool HasValidOp = true;

	while (HasValidOp)
	{
		switch (Op)
		{
		case EVectorVMOp::outputdata_half:
			ConditionalAddOutputWrapper(Context, OuterAdded);
			FScalarKernelWriteOutputIndexed<float, FFloat16, 2>::Optimize(Context);
			break;

		case EVectorVMOp::outputdata_int32:
			ConditionalAddOutputWrapper(Context, OuterAdded);
			FScalarKernelWriteOutputIndexed<int32, int32, 1>::Optimize(Context);
			break;

		case EVectorVMOp::outputdata_float:
			ConditionalAddOutputWrapper(Context, OuterAdded);
			FScalarKernelWriteOutputIndexed<float, float, 0>::Optimize(Context);
			break;

		default:
			HasValidOp = false;
			break;
		}

		if (HasValidOp)
		{
			Op = Context.BaseContext.DecodeOp();
		}
	}

	if (OuterAdded)
	{
		Context.WriteExecFunction(nullptr);
	}
	return Op;
}

// Optimization managed by GbBatchPackVMOutput via PackedOutputOptimization()
// Looks for the common pattern of an acquireindex op followed by a number of associated outputdata ops.  The
// stock operation is to write an index into a temporary register, and then have the different outputs streams
// write into the indexed location.  This optimization does a number of things:
// -first we check if 'validity' is uniform or not, if it is we can have a fast path of both figuring out how many
// indices we need, as well as how to write the output (if we find that they are all invalid, then we don't need to do anything!)
// -if we need to evaluate the validity of each element we quickly count up the number (with vector intrinsics) and
// grab a block of the indices (rather than one at a time)
// -rather than storing the indices to use, we store a int8 mask which indicates a valid flag for each of the next 4 samples
// -outputs are then written to depending on their source and their frequency:
//		-uniform sources will be splatted to all valid entries
//		-variable sources will be packed into the available slots
struct FBatchedWriteIndexedOutput
{
	// acquires a batch of indices from the provided CounterHandler.  If we're running in parallel, then we'll need to use
	// atomics to guarantee our place in the list of indices.
	template<bool bParallel>
	static VM_FORCEINLINE void AcquireCounterIndex(FVectorVMContext& Context, FDataSetCounterHandler& CounterHandler, int32 AcquireCount)
	{
		if (AcquireCount)
		{
			int32* CounterHandlerIndex = CounterHandler.Get();
			int32 StartIndex = INDEX_NONE;

			if (bParallel)
			{
				StartIndex = FPlatformAtomics::InterlockedAdd(CounterHandlerIndex, AcquireCount);
			}
			else
			{
				StartIndex = *CounterHandlerIndex;
				*CounterHandlerIndex = StartIndex + AcquireCount;
			}

			// increment StartIndex, since CounterHandlerIndex starts at INDEX_NONE
			Context.ValidInstanceIndexStart = StartIndex + 1;
		}

		Context.ValidInstanceCount = AcquireCount;
		Context.ValidInstanceUniform = !AcquireCount || (Context.NumInstances == AcquireCount);
	}

	// evaluates a register to evaluate which instances are valid or not; will read 4 entries at a time and generate a
	// a mask for which entries are valid as well as an overall count
	template<bool bParallel>
	static void HandleRegisterValidIndices(FVectorVMContext& Context)
	{
		FDataSetCounterHandler CounterHandler(Context);
		FRegisterHandler<VectorRegister4Float> ValidReader(Context);
		FRegisterHandler<int8> Dst(Context);

		int8* DestAddr = Dst.GetDest();

		// we can process VECTOR_WIDTH_FLOATS entries at a time, generating a int8 mask for each set of 4 indicating
		// which are valid
		const int32 LoopCount = FMath::DivideAndRoundUp(Context.NumInstances, VECTOR_WIDTH_FLOATS);

		int32 Remainder = Context.NumInstances;
		int32 ValidCount = 0;
		for (int32 LoopIt = 0; LoopIt < LoopCount; ++LoopIt)
		{
			// input register needs to be padded to allow for 16 byte reads; but mask out the ones beyond NumInstances
			uint8 ValidMask = static_cast<uint8>(VectorMaskBits(ValidReader.GetAndAdvance()));
			ValidMask &= ~(0xFF << FMath::Min(VECTOR_WIDTH_FLOATS, Remainder));
			ValidCount += FMath::CountBits(ValidMask);
			
			DestAddr[LoopIt] = ValidMask;
			Remainder -= VECTOR_WIDTH_FLOATS;
		}

		// grab our batch of indices
		AcquireCounterIndex<bParallel>(Context, CounterHandler, ValidCount);
	}

	// evaluates the uniform check and grab the appropriate number of indices
	template<typename ValidReaderType, bool bParallel>
	static VM_FORCEINLINE void HandleUniformValidIndices(FVectorVMContext& Context)
	{
		FDataSetCounterHandler CounterHandler(Context);
		ValidReaderType ValidReader(Context);

		if (ValidReader.Get())
		{
			AcquireCounterIndex<bParallel>(Context, CounterHandler, Context.NumInstances);
		}
	}

	template<uint8 SrcOpType>
	static VM_FORCEINLINE void IndexExecOptimized(FVectorVMContext& Context)
	{
		if (Context.IsParallelExecution())
		{
			switch (SrcOpType)
			{
			case SRCOP_RRR: HandleRegisterValidIndices<true>(Context); break;
			case SRCOP_RRC:	HandleUniformValidIndices<FConstantHandler<int32>, true>(Context); break;
			default: check(0); break;
			}
		}
		else
		{
			switch (SrcOpType)
			{
			case SRCOP_RRR: HandleRegisterValidIndices<false>(Context); break;
			case SRCOP_RRC:	HandleUniformValidIndices<FConstantHandler<int32>, false>(Context); break;
			default: check(0); break;
			}
		}
	}

	void OptimizeAcquireIndex(FVectorVMCodeOptimizerContext& Context)
	{
		const uint32 SrcOpType = Context.BaseContext.DecodeSrcOperandTypes();

		AcquireIndexConstant = !!(SrcOpType & OP0_CONST);

		switch (SrcOpType)
		{
		case SRCOP_RRR: Context.WriteExecFunction(IndexExecOptimized<SRCOP_RRR>); break;
		case SRCOP_RRC: Context.WriteExecFunction(IndexExecOptimized<SRCOP_RRC>); break;
		default: check(0); break;
		}

		DataSetCounterIndex = Context.DecodeU16();
		ValidTestRegisterIndex = Context.DecodeU16();
		WorkingRegisterIndex = Context.DecodeU16();

		Context.Write(DataSetCounterIndex);
		Context.Write(ValidTestRegisterIndex);

		// we only need the working register if we've got non-uniform data
		if (SrcOpType == SRCOP_RRR)
		{
			Context.Write(WorkingRegisterIndex);
		}
	}

	bool IsOutput(EVectorVMOp Op) const
	{
		return (Op == EVectorVMOp::outputdata_int32
			|| Op == EVectorVMOp::outputdata_float
			|| Op == EVectorVMOp::outputdata_half);
	}

	bool IsValidEnd(EVectorVMOp Op) const
	{
		return (Op == EVectorVMOp::done
			|| Op == EVectorVMOp::acquireindex);
	}

	static bool SkipIfEmpty(FVectorVMContext& Context)
	{
		if (!Context.ValidInstanceCount)
		{
			Context.SkipCode(2 * sizeof(uint16)); // don't need DataSetIndex or DestIndexRegisterIdx
			const uint16 AccumulatedOpCount = Context.DecodeU16();

			Context.SkipCode(AccumulatedOpCount * 2 * sizeof(uint16));
			return true;
		}

		return false;
	}

	VM_FORCEINLINE static void SplatElement(int32 OutputCount, int32 ElementValue, int32* RESTRICT OutputElements)
	{
		const int32 OutputCountWide = AlignDown(OutputCount, VECTOR_WIDTH_FLOATS);

		if (OutputCountWide)
		{
			const VectorRegister4Int SplatValue = MakeVectorRegisterInt(ElementValue, ElementValue, ElementValue, ElementValue);

			for (int32 i = 0; i < OutputCountWide; i += VECTOR_WIDTH_FLOATS)
			{
				VectorIntStore(SplatValue, OutputElements + i);
			}
		}

		for (int32 i = OutputCountWide; i < OutputCount; ++i)
		{
			OutputElements[i] = ElementValue;
		}
	}

	VM_FORCEINLINE static void SplatElement(int32 OutputCount, float ElementValue, float* RESTRICT OutputElements)
	{
		const int32 OutputCountWide = AlignDown(OutputCount, VECTOR_WIDTH_FLOATS);

		if (OutputCountWide)
		{
			const VectorRegister4Float SplatValue = MakeVectorRegister(ElementValue, ElementValue, ElementValue, ElementValue);

			for (int32 i = 0; i < OutputCountWide; i += VECTOR_WIDTH_FLOATS)
			{
				VectorStore(SplatValue, OutputElements + i);
			}
		}

		for (int32 i = OutputCountWide; i < OutputCount; ++i)
		{
			OutputElements[i] = ElementValue;
		}
	}


	VM_FORCEINLINE static void SplatElement(int32 OutputCount, float ElementValue, FFloat16* RESTRICT OutputElements)
	{
		uint16 TargetValue;
		FPlatformMath::StoreHalf(&TargetValue, ElementValue);
		uint32 TargetValue32 = uint32(TargetValue) | ((uint32(TargetValue) << 16));
		const VectorRegister4Float SplatValue = MakeVectorRegister(TargetValue32, TargetValue32, TargetValue32, TargetValue32);

		while (OutputCount > 7)
		{
			VectorStore(SplatValue, (float*)OutputElements); // TODO: LWC: this was using a void* previously which always assumed float*, but revisit, doesn't seem correct either way...
			OutputElements += 8;
			OutputCount -= 8;
		}

		while (OutputCount > 0)
		{
			*((uint16*)OutputElements) = TargetValue;
			OutputElements++;
			OutputCount--;
		}
	}

	template<typename T>
	VM_FORCEINLINE static void CopyElements(int32 ElementCount, const T* RESTRICT SourceElements, T* RESTRICT OutputElements)
	{
		memcpy(OutputElements, SourceElements, ElementCount * sizeof(T));
	}

	VM_FORCEINLINE static void CopyElements(int32 ElementCount, const float* RESTRICT SourceElements, FFloat16* RESTRICT OutputElements)
	{
		const float* Src = SourceElements;
		uint16* Dst = (uint16*)OutputElements;
		while (ElementCount > 7)
		{
			FPlatformMath::WideVectorStoreHalf(Dst, Src);
			Dst += 8;
			Src += 8;
			ElementCount -= 8;
		}
		while (ElementCount > 3)
		{
			FPlatformMath::VectorStoreHalf(Dst, Src);
			Dst += 4;
			Src += 4;
			ElementCount -= 4;
		}
		while (ElementCount > 0)
		{
			FPlatformMath::StoreHalf(Dst, *Src);
			Dst += 1;
			Src += 1;
			ElementCount -= 1;
		}
	}

	template<typename SourceType, typename TargetType>
	VM_FORCEINLINE static void ScalarShuffleElements(int32 ElementCount, const SourceType* RESTRICT SourceElements, const int8* RESTRICT ValidMask, TargetType* RESTRICT OutputElements)
	{
		constexpr int32 ElementsPerMask = 4;

		// scalar path that will read 4 values and write one at a time to the output based on the valid mask
		int32 MaskIt = 0;

		while (ElementCount)
		{
			const int8 ShuffleMask = ValidMask[MaskIt];
			const int8 AdvanceCount = FMath::CountBits(ShuffleMask);
			check(AdvanceCount >= 0 && AdvanceCount <= 4);
			if (AdvanceCount)
			{
				for (int32 ScalarIt = 0; ScalarIt < ElementsPerMask; ++ScalarIt)
				{
					if (!!(ShuffleMask & (1 << ScalarIt)))
					{
						*OutputElements = SourceElements[MaskIt * ElementsPerMask + ScalarIt];
						++OutputElements;
					}
				}

				ElementCount -= AdvanceCount;
			}
			++MaskIt;
		}
	}

	VM_FORCEINLINE static void ScalarShuffleElements(int32 ElementCount, const float* RESTRICT SourceElements, const int8* RESTRICT ValidMask, FFloat16* RESTRICT OutputElements)
	{
		constexpr int32 ElementsPerMask = 4;

		// scalar path that will read 4 values and write one at a time to the output based on the valid mask
		int32 MaskIt = 0;

		while (ElementCount)
		{
			checkSlow(ElementCount > 0);
			const int8 ShuffleMask = ValidMask[MaskIt];
			const int8 AdvanceCount = FMath::CountBits(ShuffleMask);
			if (AdvanceCount)
			{
				for (int32 ScalarIt = 0; ScalarIt < ElementsPerMask; ++ScalarIt)
				{
					if (!!(ShuffleMask & (1 << ScalarIt)))
					{
						FPlatformMath::StoreHalf((uint16*)OutputElements, SourceElements[MaskIt * ElementsPerMask + ScalarIt]);
						++OutputElements;
					}
				}

				ElementCount -= AdvanceCount;
			}
			++MaskIt;
		}
	}

	VM_FORCEINLINE static void VectorShuffleElements(int32 ElementCount, const int32* RESTRICT SourceElements, const int8* RESTRICT ValidMask, int32* RESTRICT OutputElements)
	{
		int32 SourceIt = 0;

		const VectorRegister4Int* RESTRICT SourceVectors = reinterpret_cast<const VectorRegister4Int* RESTRICT>(SourceElements);

		// vector shuffle path writes 4 at a time (though the trailing elements may not be valid) until we have to move over
		// to the scalar version for fear of overwriting our neighbors
		while (ElementCount >= VECTOR_WIDTH_FLOATS)
		{
			const int8 ShuffleMask = ValidMask[SourceIt];
			const int8 AdvanceCount = FMath::CountBits(ShuffleMask);
			check(AdvanceCount >= 0 && AdvanceCount <= 4);

			//
			// VectorIntStore(		- unaligned writes of 16 bytes to our Destination; note that this maneuver requires us to have
			//						our output buffers padded out to 16 bytes!
			//	VectorIntShuffle(	- swizzle our source register to pack the valid entries at the beginning, with 0s at the end
			//    Source,			- source data
			//    ShuffleMask),		- result of the VectorMaskBits done in the acquireindex, int8/VectorRegister4Float of input
			//  Destination);
			VectorIntStore(VectorIntShuffle(SourceVectors[SourceIt], VectorVMConstants::RegisterShuffleMask[ShuffleMask]), OutputElements);

			OutputElements += AdvanceCount;
			ElementCount -= AdvanceCount;

			++SourceIt;
		}

		ScalarShuffleElements(ElementCount, SourceElements + VECTOR_WIDTH_FLOATS * SourceIt, ValidMask + SourceIt, OutputElements);
	}

	VM_FORCEINLINE static void VectorShuffleElements(int32 ElementCount, const float* RESTRICT SourceElements, const int8* RESTRICT ValidMask, FFloat16* RESTRICT OutputElements)
	{
		int32 SourceIt = 0;

		const VectorRegister4Int* RESTRICT SourceVectors = reinterpret_cast<const VectorRegister4Int* RESTRICT>(SourceElements);

		// vector shuffle path writes 4 at a time (though the trailing elements may not be valid) until we have to move over
		// to the scalar version for fear of overwriting our neighbors
		while (ElementCount >= VECTOR_WIDTH_FLOATS)
		{
			const int8 ShuffleMask = ValidMask[SourceIt];
			const int8 AdvanceCount = FMath::CountBits(ShuffleMask);
			check(AdvanceCount >= 0 && AdvanceCount <= 4);

			// VectorIntStore(		- unaligned writes of 16 bytes to our Destination; note that this maneuver requires us to have
			//						our output buffers padded out to 16 bytes!
			//	VectorIntShuffle(	- swizzle our source register to pack the valid entries at the beginning, with 0s at the end
			//    Source,			- source data
			//    ShuffleMask),		- result of the VectorMaskBits done in the acquireindex, int8/VectorRegister4Float of input
			//  Destination);

			const VectorRegister4Int ShuffledFloats = VectorIntShuffle(SourceVectors[SourceIt], VectorVMConstants::RegisterShuffleMask[ShuffleMask]);
			FPlatformMath::VectorStoreHalf((uint16*)OutputElements, (float*)&ShuffledFloats);

			OutputElements += AdvanceCount;
			ElementCount -= AdvanceCount;

			++SourceIt;
		}

		ScalarShuffleElements(ElementCount, SourceElements + VECTOR_WIDTH_FLOATS * SourceIt, ValidMask + SourceIt, OutputElements);
	}

	template<typename SourceType, typename TargetType, int32 TypeOffset>
	static void CopyConstantToOutput(FVectorVMContext& Context)
	{
		if (SkipIfEmpty(Context))
		{
			return;
		}

		const uint16 DataSetIndex = Context.DecodeU16();
		Context.SkipCode(sizeof(uint16)); // don't need DestIndexRegisterIdx
		const uint16 AccumulatedOpCount = Context.DecodeU16();

		FDataSetMeta& DataSetMeta = Context.GetDataSetMeta(DataSetIndex);

		for (uint16 OpIt = 0; OpIt < AccumulatedOpCount; ++OpIt)
		{
			FConstantHandler<SourceType> SourceHandler(Context);
			const uint16 DestRegisterIndex = Context.DecodeU16();

			TargetType* RESTRICT OutputElements = Context.GetOutputRegister<TargetType, TypeOffset>(DataSetIndex, DestRegisterIndex) + Context.ValidInstanceIndexStart;

			SplatElement(Context.ValidInstanceCount, SourceHandler.Constant, OutputElements);
		}
	}

	template<typename SourceType, typename TargetType, int32 TypeOffset, bool bCheckIfEmpty = true>
	static void CopyRegisterToOutput(FVectorVMContext& Context)
	{
		if (bCheckIfEmpty && SkipIfEmpty(Context))
		{
			return;
		}

		const uint16 DataSetIndex = Context.DecodeU16();
		Context.SkipCode(sizeof(uint16)); // don't need DestIndexRegisterIdx
		const uint16 AccumulatedOpCount = Context.DecodeU16();

		check(Context.ValidInstanceCount == Context.NumInstances);
		FDataSetMeta& DataSetMeta = Context.GetDataSetMeta(DataSetIndex);

		for (uint16 OpIt = 0; OpIt < AccumulatedOpCount; ++OpIt)
		{
			FRegisterHandler<SourceType> SourceHandler(Context);
			const uint16 DestRegisterIndex = Context.DecodeU16();

			TargetType* RESTRICT OutputElements = Context.GetOutputRegister<TargetType, TypeOffset>(DataSetIndex, DestRegisterIndex) + Context.ValidInstanceIndexStart;

			CopyElements(Context.ValidInstanceCount, SourceHandler.GetDest(), OutputElements);
		}
	}

	template<typename SourceType, typename TargetType, int32 TypeOffset>
	static void ShuffleRegisterToOutput(FVectorVMContext& Context)
	{
		if (SkipIfEmpty(Context))
		{
			return;
		}
		else if (Context.ValidInstanceUniform)
		{
			CopyRegisterToOutput<SourceType, TargetType, TypeOffset, false>(Context);
			return;
		}

		const uint16 DataSetIndex = Context.DecodeU16();
		const uint16 DestIndexRegisterIdx = Context.DecodeU16();
		const uint16 AccumulatedOpCount = Context.DecodeU16();

		FDataSetMeta& DataSetMeta = Context.GetDataSetMeta(DataSetIndex);

		const int8* RESTRICT DestIndexReg = reinterpret_cast<const int8*>(Context.GetTempRegister(DestIndexRegisterIdx));

		for (uint16 OpIt = 0; OpIt < AccumulatedOpCount; ++OpIt)
		{
			FRegisterHandler<SourceType> SourceRegister(Context);
			TargetType* RESTRICT DestReg = Context.GetOutputRegister<TargetType, TypeOffset>(DataSetIndex, Context.DecodeU16()) + Context.ValidInstanceIndexStart;

			// the number of instances that we're expecting to write.  it is important that we keep track of it because when we
			// get down to the end we need to switch from the shuffled approach to a scalar approach so that we don't
			// overwrite the indexed output that another parallel context might have written to
			VectorShuffleElements(Context.ValidInstanceCount, SourceRegister.GetDest(), DestIndexReg, DestReg);
		}
	}

	bool OptimizeBatch(FVectorVMCodeOptimizerContext& Context)
	{
		const int32 BatchedOpCount = BatchedOps.Num();

		if (!BatchedOpCount)
			return false;

		for (const auto& BatchEntry : BatchedOps)
		{
			const uint16 AccumulatedOpCount = BatchEntry.Value.Num();

			if (!AccumulatedOpCount)
				continue;

			// matrix of options based on the:
			//	Op (are we outputting ints, floats or halfs)
			//	SrcOpType (are we copying a constant or a register over)
			// additionally, if we know that the index is constant, then we know at worst we're doing a copy

			if (BatchEntry.Key.SrcOpType == SRCOP_RRC)
			{
				switch (BatchEntry.Key.Op)
				{
				case EVectorVMOp::outputdata_float: Context.WriteExecFunction(CopyConstantToOutput<float, float, 0>); break;
				case EVectorVMOp::outputdata_int32: Context.WriteExecFunction(CopyConstantToOutput<int32, int32, 1>); break;
				case EVectorVMOp::outputdata_half: Context.WriteExecFunction(CopyConstantToOutput<float, FFloat16, 2>); break;
				default: check(0);
				}
			}
			else if (AcquireIndexConstant)
			{
				switch (BatchEntry.Key.Op)
				{
				case EVectorVMOp::outputdata_float: Context.WriteExecFunction(CopyRegisterToOutput<float, float, 0>); break;
				case EVectorVMOp::outputdata_int32: Context.WriteExecFunction(CopyRegisterToOutput<int32, int32, 1>); break;
				case EVectorVMOp::outputdata_half: Context.WriteExecFunction(CopyRegisterToOutput<float, FFloat16, 2>); break;
				default: check(0);
				}
			}
			else
			{
				check(BatchEntry.Key.SrcOpType == SRCOP_RRR);
				switch (BatchEntry.Key.Op)
				{
				case EVectorVMOp::outputdata_float: Context.WriteExecFunction(ShuffleRegisterToOutput<int32, int32, 0>); break;
				case EVectorVMOp::outputdata_int32: Context.WriteExecFunction(ShuffleRegisterToOutput<int32, int32, 1>); break;
				case EVectorVMOp::outputdata_half: Context.WriteExecFunction(ShuffleRegisterToOutput<float, FFloat16, 2>); break;
				default: check(0);
				}
			}

			Context.Write(BatchEntry.Key.DataSetIndex);
			Context.Write(BatchEntry.Key.DestIndexRegisterIdx);
			Context.Write(AccumulatedOpCount);
			for (const FOpValue& OpValue : BatchEntry.Value)
			{
				Context.Write(OpValue.SourceRegisterIndex);
				Context.Write(OpValue.DestRegisterIdx);
			}
		}

		return true;
	}

	bool ExtractOp(EVectorVMOp Op, FVectorVMCodeOptimizerContext& Context)
	{
		FOpKey Key;
		Key.Op = Op;
		Key.SrcOpType = Context.BaseContext.DecodeSrcOperandTypes();
		Key.DataSetIndex = Context.DecodeU16();
		Key.DestIndexRegisterIdx = Context.DecodeU16();

		if (Key.DestIndexRegisterIdx != WorkingRegisterIndex)
		{
			// if we've found an output node that is not related to the acquire index op, then just exit
			return false;
		}

		FOpValue Value;
		Value.SourceRegisterIndex = Context.DecodeU16();
		Value.DestRegisterIdx = Context.DecodeU16();

		TArray<FOpValue>& ExistingOps = BatchedOps.FindOrAdd(Key);
		ExistingOps.Add(Value);

		return true;
	}

private:
	using RegisterType = VectorRegister4Int;
	using ScalarType = int32;

	uint16 DataSetCounterIndex = 0;
	uint16 ValidTestRegisterIndex = 0;
	uint16 WorkingRegisterIndex = 0;
	bool AcquireIndexConstant = false;

	struct FOpKey
	{
		uint16 DestIndexRegisterIdx;
		uint16 DataSetIndex;
		uint8 SrcOpType;
		EVectorVMOp Op;
	};

	struct FOpValue
	{
		uint16 SourceRegisterIndex;
		uint16 DestRegisterIdx;
	};

	struct FOpKeyFuncs : public TDefaultMapKeyFuncs<FOpKey, TArray<FOpValue>, false>
	{
		static VM_FORCEINLINE bool Matches(const FOpKey& A, const FOpKey& B)
		{
			return A.DestIndexRegisterIdx == B.DestIndexRegisterIdx
				&& A.DataSetIndex == B.DataSetIndex
				&& A.SrcOpType == B.SrcOpType
				&& A.Op == B.Op;
		}

		static VM_FORCEINLINE uint32 GetKeyHash(const FOpKey& Key)
		{
			return HashCombine(Key.DestIndexRegisterIdx | (Key.DataSetIndex << 16), Key.SrcOpType | (static_cast<uint8>(Key.Op) << 8));
		}
	};

	TMap<FOpKey, TArray<FOpValue>, FDefaultSetAllocator, FOpKeyFuncs> BatchedOps;
};

// look for the pattern of acquireindex followed by a bunch of outputs.
bool PackedOutputOptimization(EVectorVMOp Op, FVectorVMCodeOptimizerContext& Context)
{
	if (!GbBatchPackVMOutput)
	{
		return false;
	}

	if (Op == EVectorVMOp::acquireindex)
	{
		const auto RollbackState = Context.CreateCodeState();

		FBatchedWriteIndexedOutput BatchedOutputOp;

		BatchedOutputOp.OptimizeAcquireIndex(Context);

		bool BatchValid = true;

		EVectorVMOp NextOp = Context.PeekOp();
		while (BatchValid && BatchedOutputOp.IsOutput(NextOp))
		{
			BatchValid = BatchedOutputOp.ExtractOp(Context.BaseContext.DecodeOp(), Context);
			NextOp = Context.PeekOp();
		}

		// if there's nothing worth optimizing here, then just revert what we've parsed
		if (!BatchValid || !BatchedOutputOp.OptimizeBatch(Context))
		{
			Context.RollbackCodeState(RollbackState);
			return false;
		}

		// We handled the existing op
		return true;
	}

	return false;
}

void ExecBatchedInput(FVectorVMContext& Context)
{
	while (true)
	{
		FVectorVMExecFunction ExecFunction = reinterpret_cast<FVectorVMExecFunction>(Context.DecodePtr());
		if (ExecFunction == nullptr)
		{
			break;
		}
		ExecFunction(Context);
	}
}

void ConditionalAddInputWrapper(FVectorVMCodeOptimizerContext& Context, bool& OuterAdded)
{
	if (!OuterAdded)
	{
		Context.WriteExecFunction(ExecBatchedInput);
		OuterAdded = true;
	}
}

EVectorVMOp BatchedInputOptimization(EVectorVMOp Op, FVectorVMCodeOptimizerContext& Context)
{
	if (!GbBatchVMInput)
	{
		return Op;
	}

	bool OuterAdded = false;
	bool HasValidOp = true;

	while (HasValidOp)
	{
		switch (Op)
		{
		case EVectorVMOp::inputdata_half:
			ConditionalAddInputWrapper(Context, OuterAdded);
			FVectorKernelReadInput<FFloat16, 2>::Optimize(Context);
			break;

		case EVectorVMOp::inputdata_int32:
			ConditionalAddInputWrapper(Context, OuterAdded);
			FVectorKernelReadInput<int32, 1>::Optimize(Context);
			break;

		case EVectorVMOp::inputdata_float:
			ConditionalAddInputWrapper(Context, OuterAdded);
			FVectorKernelReadInput<float, 0>::Optimize(Context);
			break;

		default:
			HasValidOp = false;
			break;
		}

		if (HasValidOp)
		{
			Op = Context.BaseContext.DecodeOp();
		}
	}

	if (OuterAdded)
	{
		Context.WriteExecFunction(nullptr);
	}
	return Op;
}

bool SafeMathOptimization(EVectorVMOp Op, FVectorVMCodeOptimizerContext& Context)
{
	if (!GbSafeOptimizedKernels)
	{
		return false;
	}

	switch (Op)
	{
		case EVectorVMOp::div: FVectorKernelDivSafe::Optimize(Context); return true;
		case EVectorVMOp::rcp: FVectorKernelRcpSafe::Optimize(Context); return true;
		case EVectorVMOp::rsq: FVectorKernelRsqSafe::Optimize(Context); return true;
		case EVectorVMOp::sqrt: FVectorKernelSqrtSafe::Optimize(Context); return true;
		case EVectorVMOp::log: FVectorKernelLogSafe::Optimize(Context); return true;
		case EVectorVMOp::pow: FVectorKernelPowSafe::Optimize(Context); return true;
		default:
			return false;
	}
}

void VectorVM::OptimizeByteCode(const uint8* ByteCode, TArray<uint8>& OptimizedCode, TArrayView<uint8> ExternalFunctionRegisterCounts)
{
	OptimizedCode.Empty();

	//-TODO:7 Support unaligned writes & little endian
#if PLATFORM_SUPPORTS_UNALIGNED_LOADS && PLATFORM_LITTLE_ENDIAN

	if (!GbOptimizeVMByteCode || (ByteCode == nullptr))
	{
		return;
	}

	FVectorVMCodeOptimizerContext Context(FVectorVMContext::Get(), ByteCode, OptimizedCode, ExternalFunctionRegisterCounts);

	// add any optimization filters in here, useful so what we can isolate optimizations with CVars
	FVectorVMCodeOptimizerContext::OptimizeVMFunction VMFilters[] =
	{
		//BatchedInputOptimization,
		//BatchedOutputOptimization,
		PackedOutputOptimization,
		SafeMathOptimization,
	};

	EVectorVMOp Op = EVectorVMOp::done;
	do
	{
		Op = Context.BaseContext.DecodeOp();

		// Filters allow us to modify a single or series of operations
		bool bOpWasFiltered = false;
		for (auto Filter : VMFilters)
		{
			if ( Filter(Op, Context) )
			{
				bOpWasFiltered = true;
				break;
			}
		}
		if (bOpWasFiltered)
		{
			continue;
		}

		// Optimize op
		switch (Op)
		{
			case EVectorVMOp::add: FVectorKernelAdd::Optimize(Context); break;
			case EVectorVMOp::sub: FVectorKernelSub::Optimize(Context); break;
			case EVectorVMOp::mul: FVectorKernelMul::Optimize(Context); break;
			case EVectorVMOp::div: FVectorKernelDiv::Optimize(Context); break;
			case EVectorVMOp::mad: FVectorKernelMad::Optimize(Context); break;
			case EVectorVMOp::lerp: FVectorKernelLerp::Optimize(Context); break;
			case EVectorVMOp::rcp: FVectorKernelRcp::Optimize(Context); break;
			case EVectorVMOp::rsq: FVectorKernelRsq::Optimize(Context); break;
			case EVectorVMOp::sqrt: FVectorKernelSqrt::Optimize(Context); break;
			case EVectorVMOp::neg: FVectorKernelNeg::Optimize(Context); break;
			case EVectorVMOp::abs: FVectorKernelAbs::Optimize(Context); break;
			case EVectorVMOp::exp: FVectorKernelExp::Optimize(Context); break;
			case EVectorVMOp::exp2: FVectorKernelExp2::Optimize(Context); break;
			case EVectorVMOp::log: FVectorKernelLog::Optimize(Context); break;
			case EVectorVMOp::log2: FVectorKernelLog2::Optimize(Context); break;
			case EVectorVMOp::sin: FVectorKernelSin::Optimize(Context); break;
			case EVectorVMOp::cos: FVectorKernelCos::Optimize(Context); break;
			case EVectorVMOp::tan: FVectorKernelTan::Optimize(Context); break;
			case EVectorVMOp::asin: FVectorKernelASin::Optimize(Context); break;
			case EVectorVMOp::acos: FVectorKernelACos::Optimize(Context); break;
			case EVectorVMOp::atan: FVectorKernelATan::Optimize(Context); break;
			case EVectorVMOp::atan2: FVectorKernelATan2::Optimize(Context); break;
			case EVectorVMOp::ceil: FVectorKernelCeil::Optimize(Context); break;
			case EVectorVMOp::floor: FVectorKernelFloor::Optimize(Context); break;
			case EVectorVMOp::round: FVectorKernelRound::Optimize(Context); break;
			case EVectorVMOp::fmod: FVectorKernelMod::Optimize(Context); break;
			case EVectorVMOp::frac: FVectorKernelFrac::Optimize(Context); break;
			case EVectorVMOp::trunc: FVectorKernelTrunc::Optimize(Context); break;
			case EVectorVMOp::clamp: FVectorKernelClamp::Optimize(Context); break;
			case EVectorVMOp::min: FVectorKernelMin::Optimize(Context); break;
			case EVectorVMOp::max: FVectorKernelMax::Optimize(Context); break;
			case EVectorVMOp::pow: FVectorKernelPow::Optimize(Context); break;
			case EVectorVMOp::sign: FVectorKernelSign::Optimize(Context); break;
			case EVectorVMOp::step: FVectorKernelStep::Optimize(Context); break;
			case EVectorVMOp::random: FVectorKernelRandom::Optimize(Context); break;
			case EVectorVMOp::noise: VectorVMNoise::Optimize_Noise1D(Context); break;
			case EVectorVMOp::noise2D: VectorVMNoise::Optimize_Noise2D(Context); break;
			case EVectorVMOp::noise3D: VectorVMNoise::Optimize_Noise3D(Context); break;

			case EVectorVMOp::cmplt: FVectorKernelCompareLT::Optimize(Context); break;
			case EVectorVMOp::cmple: FVectorKernelCompareLE::Optimize(Context); break;
			case EVectorVMOp::cmpgt: FVectorKernelCompareGT::Optimize(Context); break;
			case EVectorVMOp::cmpge: FVectorKernelCompareGE::Optimize(Context); break;
			case EVectorVMOp::cmpeq: FVectorKernelCompareEQ::Optimize(Context); break;
			case EVectorVMOp::cmpneq: FVectorKernelCompareNEQ::Optimize(Context); break;
			case EVectorVMOp::select: FVectorKernelSelect::Optimize(Context); break;

			case EVectorVMOp::addi: FVectorIntKernelAdd::Optimize(Context); break;
			case EVectorVMOp::subi: FVectorIntKernelSubtract::Optimize(Context); break;
			case EVectorVMOp::muli: FVectorIntKernelMultiply::Optimize(Context); break;
			case EVectorVMOp::divi: FVectorIntKernelDivide::Optimize(Context); break;
			case EVectorVMOp::clampi: FVectorIntKernelClamp::Optimize(Context); break;
			case EVectorVMOp::mini: FVectorIntKernelMin::Optimize(Context); break;
			case EVectorVMOp::maxi: FVectorIntKernelMax::Optimize(Context); break;
			case EVectorVMOp::absi: FVectorIntKernelAbs::Optimize(Context); break;
			case EVectorVMOp::negi: FVectorIntKernelNegate::Optimize(Context); break;
			case EVectorVMOp::signi: FVectorIntKernelSign::Optimize(Context); break;
			case EVectorVMOp::randomi: FScalarIntKernelRandom::Optimize(Context); break;
			case EVectorVMOp::cmplti: FVectorIntKernelCompareLT::Optimize(Context); break;
			case EVectorVMOp::cmplei: FVectorIntKernelCompareLE::Optimize(Context); break;
			case EVectorVMOp::cmpgti: FVectorIntKernelCompareGT::Optimize(Context); break;
			case EVectorVMOp::cmpgei: FVectorIntKernelCompareGE::Optimize(Context); break;
			case EVectorVMOp::cmpeqi: FVectorIntKernelCompareEQ::Optimize(Context); break;
			case EVectorVMOp::cmpneqi: FVectorIntKernelCompareNEQ::Optimize(Context); break;
			case EVectorVMOp::bit_and: FVectorIntKernelBitAnd::Optimize(Context); break;
			case EVectorVMOp::bit_or: FVectorIntKernelBitOr::Optimize(Context); break;
			case EVectorVMOp::bit_xor: FVectorIntKernelBitXor::Optimize(Context); break;
			case EVectorVMOp::bit_not: FVectorIntKernelBitNot::Optimize(Context); break;
			case EVectorVMOp::bit_lshift: FVectorIntKernelBitLShift::Optimize(Context); break;
			case EVectorVMOp::bit_rshift: FVectorIntKernelBitRShift::Optimize(Context); break;
			case EVectorVMOp::logic_and: FVectorIntKernelLogicAnd::Optimize(Context); break;
			case EVectorVMOp::logic_or: FVectorIntKernelLogicOr::Optimize(Context); break;
			case EVectorVMOp::logic_xor: FVectorIntKernelLogicXor::Optimize(Context); break;
			case EVectorVMOp::logic_not: FVectorIntKernelLogicNot::Optimize(Context); break;
			case EVectorVMOp::f2i: FVectorKernelFloatToInt::Optimize(Context); break;
			case EVectorVMOp::i2f: FVectorKernelIntToFloat::Optimize(Context); break;
			case EVectorVMOp::f2b: FVectorKernelFloatToBool::Optimize(Context); break;
			case EVectorVMOp::b2f: FVectorKernelBoolToFloat::Optimize(Context); break;
			case EVectorVMOp::i2b: FVectorKernelIntToBool::Optimize(Context); break;
			case EVectorVMOp::b2i: FVectorKernelBoolToInt::Optimize(Context); break;
			case EVectorVMOp::fasi: FVectorKernelFloatAsInt::Optimize(Context); break;
			case EVectorVMOp::iasf: FVectorKernelIntAsFloat::Optimize(Context); break;

			case EVectorVMOp::outputdata_half:	FScalarKernelWriteOutputIndexed<float, FFloat16, 2>::Optimize(Context);	break;
			case EVectorVMOp::inputdata_half: FVectorKernelReadInput<FFloat16, 2>::Optimize(Context); break;
			case EVectorVMOp::outputdata_int32:	FScalarKernelWriteOutputIndexed<int32, int32, 1>::Optimize(Context);	break;
			case EVectorVMOp::inputdata_int32: FVectorKernelReadInput<int32, 1>::Optimize(Context); break;
			case EVectorVMOp::outputdata_float:	FScalarKernelWriteOutputIndexed<float, float, 0>::Optimize(Context);	break;
			case EVectorVMOp::inputdata_float: FVectorKernelReadInput<float, 0>::Optimize(Context); break;
			case EVectorVMOp::inputdata_noadvance_int32: FVectorKernelReadInputNoAdvance<int32, 1>::Optimize(Context); break;
			case EVectorVMOp::inputdata_noadvance_float: FVectorKernelReadInputNoAdvance<float, 0>::Optimize(Context); break;
			case EVectorVMOp::inputdata_noadvance_half: FVectorKernelReadInputNoAdvance<FFloat16, 2>::Optimize(Context); break;
			case EVectorVMOp::acquireindex: FScalarKernelAcquireCounterIndex::Optimize(Context); break;
			case EVectorVMOp::external_func_call: FKernelExternalFunctionCall::Optimize(Context); break;

			case EVectorVMOp::exec_index: FVectorKernelExecutionIndex::Optimize(Context); break;

			case EVectorVMOp::enter_stat_scope: FVectorKernelEnterStatScope::Optimize(Context); break;
			case EVectorVMOp::exit_stat_scope: FVectorKernelExitStatScope::Optimize(Context); break;

			//Special case ops to handle unique IDs but this can be written as generalized buffer operations. TODO!
			case EVectorVMOp::update_id:	FScalarKernelUpdateID::Optimize(Context); break;
			case EVectorVMOp::acquire_id:	FScalarKernelAcquireID::Optimize(Context); break;

			// Execution always terminates with a "done" opcode.
			case EVectorVMOp::done:
				break;

				// Opcode not recognized / implemented.
			default:
				UE_LOG(LogVectorVM, Fatal, TEXT("Unknown op code 0x%02x"), (uint32)Op);
				OptimizedCode.Empty();
				return;//BAIL
		}
	} while (Op != EVectorVMOp::done);
	Context.WriteExecFunction(nullptr);

	Context.EncodeJumpTable();
#endif //PLATFORM_SUPPORTS_UNALIGNED_LOADS && PLATFORM_LITTLE_ENDIAN
}

#endif // #if VECTORVM_SUPPORTS_LEGACY

#undef VM_FORCEINLINE
