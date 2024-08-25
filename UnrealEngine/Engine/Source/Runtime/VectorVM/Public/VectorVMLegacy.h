// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/ThreadSingleton.h"
#include "Math/RandomStream.h"
#include "VectorVMCommon.h"

#if VECTORVM_SUPPORTS_LEGACY

//Data the VM will keep on each dataset locally per thread which is then thread safely pushed to it's destination at the end of execution.
struct FDataSetThreadLocalTempData
{
	FDataSetThreadLocalTempData()
	{
		Reset();
	}

	FORCEINLINE void Reset()
	{
		IDsToFree.Reset();
		MaxID = INDEX_NONE;
	}

	TArray<int32> IDsToFree;
	int32 MaxID;

	//TODO: Possibly store output data locally and memcpy to the real buffers. Could avoid false sharing in parallel execution and so improve perf.
	//using _mm_stream_ps on platforms that support could also work for this?
	//TArray<TArray<float>> OutputFloatData;
	//TArray<TArray<int32>> OutputIntData;
};


/**
* Context information passed around during VM execution.
*/
struct FVectorVMContext : TThreadSingleton<FVectorVMContext>
{
private:

	friend struct FVectorVMCodeOptimizerContext;

public:

	/** Pointer to the next element in the byte code. */
	uint8 const* RESTRICT Code;

	/** Pointer to the constant table. */
	const uint8* const* RESTRICT ConstantTable;
	const int32* ConstantTableSizes;
	int32 ConstantTableCount;

	/** Num temp registers required by this script. */
	int32 NumTempRegisters;

	/** Pointer to the shared data table. */
	const FVMExternalFunction* const* RESTRICT ExternalFunctionTable;
	/** Table of user pointers.*/
	void** UserPtrTable;

	/** Number of instances to process. */
	int32 NumInstances;
	/** Number of instances to process when doing batches of VECTOR_WIDTH_FLOATS. */
	int32 NumInstancesVectorFloats;
	/** Start instance of current chunk. */
	int32 StartInstance;

	/** HACK: An additional instance offset to allow external functions direct access to specific instances in the buffers. */
	int32 ExternalFunctionInstanceOffset;

	/** Array of meta data on data sets. TODO: This struct should be removed and all features it contains be handled by more general vm ops and the compiler's knowledge of offsets etc. */
	TArrayView<FDataSetMeta> DataSetMetaTable;

	TArray<FDataSetThreadLocalTempData> ThreadLocalTempData;

#if STATS
	TArray<FStatStackEntry, TInlineAllocator<64>> StatCounterStack;
	TArrayView<FStatScopeData> StatScopes;
	TArray<uint64, TInlineAllocator<64>> ScopeExecCycles;
#elif ENABLE_STATNAMEDEVENTS
	TArrayView<const FString> StatNamedEventScopes;
#endif

	TArray<uint8, TAlignedHeapAllocator<VECTOR_WIDTH_BYTES>> TempRegTable;
	uint32 TempRegisterSize;
	uint32 TempBufferSize;

	/** Thread local random stream for use in external functions needing non-deterministic randoms. */
	FRandomStream RandStream;

	/** Thread local per instance random counters for use in external functions needing deterministic randoms. */
	TArray<int32> RandCounters;

	bool bIsParallelExecution;

	int32 ValidInstanceIndexStart = INDEX_NONE;
	int32 ValidInstanceCount = 0;
	bool ValidInstanceUniform = false;

	FVectorVMContext();

	void PrepareForExec(
		int32 InNumTempRegisters,
		int32 ConstantTableCount,
		const uint8* const* InConstantTables,
		const int32* InConstantTableSizes,
		const FVMExternalFunction* const* InExternalFunctionTable,
		void** InUserPtrTable,
		TArrayView<FDataSetMeta> InDataSetMetaTable,
		int32 MaxNumInstances,
		bool bInParallelExecution);

#if STATS
	void SetStatScopes(TArrayView<FStatScopeData> InStatScopes);
#elif ENABLE_STATNAMEDEVENTS
	void SetStatNamedEventScopes(TArrayView<const FString> InStatNamedEventScopes);
#endif

	void FinishExec();

	void PrepareForChunk(const uint8* InCode, int32 InNumInstances, int32 InStartInstance)
	{
		Code = InCode;
		NumInstances = InNumInstances;
		NumInstancesVectorFloats = (NumInstances + VECTOR_WIDTH_FLOATS - 1) / VECTOR_WIDTH_FLOATS;
		StartInstance = InStartInstance;

		ExternalFunctionInstanceOffset = 0;

		ValidInstanceCount = 0;
		ValidInstanceIndexStart = INDEX_NONE;
		ValidInstanceUniform = false;

		RandCounters.Reset();
		RandCounters.SetNumZeroed(InNumInstances);
	}

	FORCEINLINE FDataSetMeta& GetDataSetMeta(int32 DataSetIndex) { return DataSetMetaTable[DataSetIndex]; }
	FORCEINLINE uint8* RESTRICT GetTempRegister(int32 RegisterIndex) { return TempRegTable.GetData() + TempRegisterSize * RegisterIndex; }
	template<typename T, int TypeOffset>
	FORCEINLINE T* RESTRICT GetInputRegister(int32 DataSetIndex, int32 RegisterIndex)
	{
		FDataSetMeta& Meta = GetDataSetMeta(DataSetIndex);
		uint32 Offset = Meta.InputRegisterTypeOffsets[TypeOffset];
		return ((T*)Meta.InputRegisters[Offset + RegisterIndex]) + Meta.InstanceOffset;
	}
	template<typename T, int TypeOffset>
	FORCEINLINE T* RESTRICT GetOutputRegister(int32 DataSetIndex, int32 RegisterIndex)
	{
		FDataSetMeta& Meta = GetDataSetMeta(DataSetIndex);
		uint32 Offset = Meta.OutputRegisterTypeOffsets[TypeOffset];
		return  ((T*)Meta.OutputRegisters[Offset + RegisterIndex]) + Meta.InstanceOffset;
	}

	int32 GetNumInstances() const { return NumInstances; }
	int32 GetStartInstance() const { return StartInstance; }

	template<uint32 InstancesPerOp>
	int32 GetNumLoops() const { return (InstancesPerOp == VECTOR_WIDTH_FLOATS) ? NumInstancesVectorFloats : ((InstancesPerOp == 1) ? NumInstances : Align(NumInstances, InstancesPerOp)); }

	FORCEINLINE uint8 DecodeU8() { return *Code++; }
#if PLATFORM_SUPPORTS_UNALIGNED_LOADS
	FORCEINLINE uint16 DecodeU16() { uint16 v = *reinterpret_cast<const uint16*>(Code); Code += sizeof(uint16); return INTEL_ORDER16(v); }
	FORCEINLINE uint32 DecodeU32() { uint32 v = *reinterpret_cast<const uint32*>(Code); Code += sizeof(uint32); return INTEL_ORDER32(v); }
	FORCEINLINE uint64 DecodeU64() { uint64 v = *reinterpret_cast<const uint64*>(Code); Code += sizeof(uint64); return INTEL_ORDER64(v); }
#else
	FORCEINLINE uint16 DecodeU16() { uint16 v = Code[1]; v = static_cast<uint16>(v << 8 | Code[0]); Code += 2; return INTEL_ORDER16(v); }
	FORCEINLINE uint32 DecodeU32() { uint32 v = Code[3]; v = v << 8 | Code[2]; v = v << 8 | Code[1]; v = v << 8 | Code[0]; Code += 4; return INTEL_ORDER32(v); }
	FORCEINLINE uint64 DecodeU64() { uint64 v = Code[7]; v = v << 8 | Code[6]; v = v << 8 | Code[5]; v = v << 8 | Code[4]; v = v << 8 | Code[3]; v = v << 8 | Code[2]; v = v << 8 | Code[1]; v = v << 8 | Code[0]; Code += 8; return INTEL_ORDER64(v); }
#endif
	FORCEINLINE uintptr_t DecodePtr() { return (sizeof(uintptr_t) == 4) ? DecodeU32() : DecodeU64(); }
	FORCEINLINE void SkipCode(int64 Count) { Code += Count; }

	/** Decode the next operation contained in the bytecode. */
	FORCEINLINE EVectorVMOp DecodeOp()
	{
		return static_cast<EVectorVMOp>(DecodeU8());
	}

	FORCEINLINE uint8 DecodeSrcOperandTypes()
	{
		return DecodeU8();
	}

	FORCEINLINE bool IsParallelExecution()
	{
		return bIsParallelExecution;
	}

	template<typename T = uint8>
	FORCEINLINE const T* GetConstant(int32 TableIndex, int32 TableOffset) const
	{
		check(TableIndex < ConstantTableCount);
		return reinterpret_cast<const T*>(ConstantTable[TableIndex] + TableOffset);
	}

	template<typename T = uint8>
	FORCEINLINE const T* GetConstant(int32 Offset) const
	{
		int32 TableIndex = 0;

		while (Offset >= ConstantTableSizes[TableIndex])
		{
			Offset -= ConstantTableSizes[TableIndex];
			++TableIndex;
		}

		check(TableIndex < ConstantTableCount);
		check(Offset < ConstantTableSizes[TableIndex]);
		return reinterpret_cast<const T*>(ConstantTable[TableIndex] + Offset);
	}
};

class FVectorVMExternalFunctionContextLegacy
{
	friend struct FNiagaraSystemScriptExecutionContext; //@NOTE(smcgrath): required for the PerInstanceFunctionHook() in the non-experimental version of VectorVM
public:
	FVectorVMExternalFunctionContextLegacy() = default;
	FVectorVMExternalFunctionContextLegacy(FVectorVMContext* InVectorVMContext) : VectorVMContext(InVectorVMContext) { }
	FORCEINLINE int32 GetStartInstance() const { return VectorVMContext->GetStartInstance(); }
	FORCEINLINE int32 GetNumInstances() const { return VectorVMContext->GetNumInstances(); }
	FORCEINLINE TArray<int32>& GetRandCounters() { return VectorVMContext->RandCounters; }
	FORCEINLINE FRandomStream& GetRandStream() { return VectorVMContext->RandStream; }

	FORCEINLINE uint8* RESTRICT GetTempRegister(int32 RegisterIndex) { return VectorVMContext->GetTempRegister(RegisterIndex); }
	FORCEINLINE uint8 DecodeU8() { return VectorVMContext->DecodeU8(); }
	FORCEINLINE uint16 DecodeU16() { return VectorVMContext->DecodeU16(); }
	FORCEINLINE uint32 DecodeU32() { return VectorVMContext->DecodeU32(); }
	FORCEINLINE uint64 DecodeU64() { return VectorVMContext->DecodeU64(); }
	template<typename T = uint8>
	FORCEINLINE const T* GetConstant(int32 TableIndex, int32 TableOffset) const { return VectorVMContext->GetConstant<T>(TableIndex, TableOffset); }
	template<typename T = uint8>
	FORCEINLINE const T* GetConstant(int32 Offset) const { return VectorVMContext->GetConstant<T>(Offset); }
	FORCEINLINE void* GetUserPtrTable(int32 UserPtrIdx) { return VectorVMContext->UserPtrTable[UserPtrIdx]; }
	FORCEINLINE int32 GetExternalFunctionInstanceOffset() const { return VectorVMContext->ExternalFunctionInstanceOffset; }
	FORCEINLINE int32 GetNumTempRegisters() const { return VectorVMContext->NumTempRegisters; }

	template<uint32 InstancesPerOp>
	FORCEINLINE int32 GetNumLoops() const { return VectorVMContext->GetNumLoops<InstancesPerOp>(); }

private:
	FVectorVMContext* VectorVMContext;
};


#endif // VECTORVM_SUPPORTS_LEGACY
