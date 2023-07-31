// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RHI.h"
#include "VectorVM.h"
#include "RenderingThread.h"
#include "NiagaraDataSet.generated.h"

/** Helper class defining the layout and location of an FNiagaraVariable in an FNiagaraDataBuffer-> */
USTRUCT()
struct FNiagaraVariableLayoutInfo
{
	GENERATED_BODY()

	/** Start index for the float components in the main buffer. */
	UPROPERTY()
	uint32 FloatComponentStart = 0;

	/** Start index for the int32 components in the main buffer. */
	UPROPERTY()
	uint32 Int32ComponentStart = 0;

	/** Start index for the half components in the main buffer. */
	UPROPERTY()
	uint32 HalfComponentStart = 0;

	uint32 GetNumFloatComponents()const { return LayoutInfo.FloatComponentByteOffsets.Num(); }
	uint32 GetNumInt32Components()const { return LayoutInfo.Int32ComponentByteOffsets.Num(); }
	uint32 GetNumHalfComponents()const { return LayoutInfo.HalfComponentByteOffsets.Num(); }

	/** This variable's type layout info. */
	UPROPERTY()
	FNiagaraTypeLayoutInfo LayoutInfo;
};

class FNiagaraDataSet;
class FNiagaraShader;
class FNiagaraGPUInstanceCountManager;
class FNiagaraGpuComputeDispatchInterface;
struct FNiagaraComputeExecutionContext;

//Base class for objects in Niagara that are owned by one object but are then passed for reading to other objects, potentially on other threads.
//This class allows us to know if the object is being used so we do not overwrite it and to ensure it's lifetime so we do not access freed data.
class FNiagaraSharedObject
{
public:
	FNiagaraSharedObject()
		: ReadRefCount(0)
	{}

	/** The owner of this object is now done with it but it may still be in use by others, possibly on other threads. Add to the deletion queue so it can be safely freed when it's no longer in use. */
	void Destroy();
	static void FlushDeletionList();

	FORCEINLINE bool IsInUse()const { return ReadRefCount.Load() != 0; }
	FORCEINLINE bool IsBeingRead()const { return ReadRefCount.Load() > 0; }
	FORCEINLINE bool IsBeingWritten()const { return ReadRefCount.Load() == INDEX_NONE; }

	FORCEINLINE void AddReadRef()
	{
		check(!IsBeingWritten());
		ReadRefCount++;
	}

	FORCEINLINE void ReleaseReadRef()
	{
		check(IsBeingRead());
		ReadRefCount--;
	}

	FORCEINLINE bool TryLock()
	{
		//Only lock if we have no readers.
		//Using INDEX_NONE as a special case value for write locks.
		int32 Expected = 0;
		return ReadRefCount.CompareExchange(Expected, INDEX_NONE);
	}

	FORCEINLINE void Unlock()
	{
		int32 Expected = INDEX_NONE;
		ensureAlwaysMsgf(ReadRefCount.CompareExchange(Expected, 0), TEXT("Trying to release a write lock on a Niagara shared object that is not locked for write."));
	}

protected:

	/**
	Count of other object currently reading this data. Keeps us from writing to or deleting this data while it's in use. These reads can be on any thread so atomic is used.
	INDEX_NONE used as special case marking this object as locked for write.
	*/
	TAtomic<int32> ReadRefCount;

	static FCriticalSection CritSec;
	static TArray<FNiagaraSharedObject*> DeferredDeletionList;

	virtual ~FNiagaraSharedObject() {}
};

/** Buffer containing one frame of Niagara simulation data. */
class NIAGARA_API FNiagaraDataBuffer : public FNiagaraSharedObject
{
	friend class FScopedNiagaraDataSetGPUReadback;
	//friend class FNiagaraGpuComputeDispatchInterface;
	
protected:
	virtual ~FNiagaraDataBuffer();

public:
	FNiagaraDataBuffer(FNiagaraDataSet* InOwner);
	void Allocate(uint32 NumInstances, bool bMaintainExisting = false);
	void ReleaseCPU();

	void AllocateGPU(FRHICommandList& RHICmdList, uint32 InNumInstances, ERHIFeatureLevel::Type FeatureLevel, const TCHAR* DebugSimName);
	void SwapGPU(FNiagaraDataBuffer* BufferToSwap);
	void ReleaseGPU();

	void SwapInstances(uint32 OldIndex, uint32 NewIndex);
	void KillInstance(uint32 InstanceIdx);
	void CopyTo(FNiagaraDataBuffer& DestBuffer, int32 SrcStartIdx, int32 DestStartIdx, int32 NumInstances)const;
	void GPUCopyFrom(const float* GPUReadBackFloat, const int* GPUReadBackInt, const FFloat16* GPUReadBackHalf, int32 StartIdx, int32 NumInstances, uint32 InSrcFloatStride, uint32 InSrcIntStride, uint32 InSrcHalfStride);
	void Dump(int32 StartIndex, int32 NumInstances, const FString& Label, const FName& SortParameterKey = FName())const;

	FORCEINLINE TArrayView<uint8 const* RESTRICT const> GetRegisterTable() const { return TArrayView<uint8 const* RESTRICT const>(RegisterTable); }
	
	typedef uint32 RegisterTypeOffsetType[3];
	FORCEINLINE const RegisterTypeOffsetType& GetRegisterTypeOffsets() { return RegisterTypeOffsets; }

	FORCEINLINE const TArray<uint8>& GetFloatBuffer() const { return FloatData; }
	FORCEINLINE const TArray<uint8>& GetInt32Buffer() const { return Int32Data; }
	FORCEINLINE const TArray<uint8>& GetHalfBuffer() const { return HalfData; }

	FORCEINLINE const uint8* GetComponentPtrFloat(uint32 ComponentIdx) const { return FloatData.GetData() + FloatStride * ComponentIdx; }
	FORCEINLINE const uint8* GetComponentPtrInt32(uint32 ComponentIdx) const { return Int32Data.GetData() + Int32Stride * ComponentIdx; }
	FORCEINLINE const uint8* GetComponentPtrHalf(uint32 ComponentIdx) const { return HalfData.GetData() + HalfStride * ComponentIdx; }
	FORCEINLINE uint8* GetComponentPtrFloat(uint32 ComponentIdx) { return FloatData.GetData() + FloatStride * ComponentIdx; }
	FORCEINLINE uint8* GetComponentPtrInt32(uint32 ComponentIdx) { return Int32Data.GetData() + Int32Stride * ComponentIdx;	}
	FORCEINLINE uint8* GetComponentPtrHalf(uint32 ComponentIdx) { return HalfData.GetData() + HalfStride * ComponentIdx; }

	FORCEINLINE float* GetInstancePtrFloat(uint32 ComponentIdx, uint32 InstanceIdx)	{ return (float*)(GetComponentPtrFloat(ComponentIdx)) + InstanceIdx; }
	FORCEINLINE int32* GetInstancePtrInt32(uint32 ComponentIdx, uint32 InstanceIdx)	{ return (int32*)(GetComponentPtrInt32(ComponentIdx)) + InstanceIdx; }
	FORCEINLINE FFloat16* GetInstancePtrHalf(uint32 ComponentIdx, uint32 InstanceIdx) { return (FFloat16*)(GetComponentPtrHalf(ComponentIdx)) + InstanceIdx; }

	FORCEINLINE const float* GetInstancePtrFloat(uint32 ComponentIdx, uint32 InstanceIdx) const { return (float*)(GetComponentPtrFloat(ComponentIdx)) + InstanceIdx; }
	FORCEINLINE const int32* GetInstancePtrInt32(uint32 ComponentIdx, uint32 InstanceIdx) const { return (int32*)(GetComponentPtrInt32(ComponentIdx)) + InstanceIdx; }
	FORCEINLINE const FFloat16* GetInstancePtrHalf(uint32 ComponentIdx, uint32 InstanceIdx) const { return (FFloat16*)(GetComponentPtrHalf(ComponentIdx)) + InstanceIdx; }

	FORCEINLINE uint8* GetComponentPtrFloat(float* BasePtr, uint32 ComponentIdx) const { return (uint8*)BasePtr + FloatStride * ComponentIdx; }
	FORCEINLINE uint8* GetComponentPtrInt32(int* BasePtr, uint32 ComponentIdx) const { return (uint8*)BasePtr + Int32Stride * ComponentIdx; }
	FORCEINLINE uint8* GetComponentPtrHalf(FFloat16* BasePtr, uint32 ComponentIdx) const { return (uint8*)(BasePtr) + HalfStride * ComponentIdx; }

	FORCEINLINE float* GetInstancePtrFloat(float* BasePtr, uint32 ComponentIdx, uint32 InstanceIdx)const { return (float*)GetComponentPtrFloat(BasePtr, ComponentIdx) + InstanceIdx; }
	FORCEINLINE int32* GetInstancePtrInt32(int* BasePtr, uint32 ComponentIdx, uint32 InstanceIdx)const { return (int32*)GetComponentPtrInt32(BasePtr, ComponentIdx) + InstanceIdx; }
	FORCEINLINE FFloat16* GetInstancePtrHalf(FFloat16 *BasePtr, uint32 ComponentIdx, uint32 InstanceIdx)const { return (FFloat16*)GetComponentPtrHalf(BasePtr, ComponentIdx) + InstanceIdx; }

	FORCEINLINE uint32 GetNumInstances()const { return NumInstances; }
	FORCEINLINE uint32 GetNumInstancesAllocated()const { return NumInstancesAllocated; }
	FORCEINLINE uint32 GetNumSpawnedInstances() const { return NumSpawnedInstances; }

	FORCEINLINE void SetNumInstances(uint32 InNumInstances) { check(InNumInstances <= NumInstancesAllocated); NumInstances = InNumInstances; }
	FORCEINLINE void SetNumSpawnedInstances(uint32 InNumSpawnedInstances) { NumSpawnedInstances = InNumSpawnedInstances; }

	FORCEINLINE void SetGPUDataReadyStage(ENiagaraGpuComputeTickStage::Type InReadyStage) { GPUDataReadyStage = InReadyStage; }
	FORCEINLINE ENiagaraGpuComputeTickStage::Type GetGPUDataReadyStage() const { return GPUDataReadyStage; }
	FORCEINLINE FRWBuffer& GetGPUBufferFloat() { return GPUBufferFloat; }
	FORCEINLINE FRWBuffer& GetGPUBufferInt() { return GPUBufferInt; }
	FORCEINLINE FRWBuffer& GetGPUBufferHalf() { return GPUBufferHalf; }
	FORCEINLINE uint32 GetGPUInstanceCountBufferOffset() const { return GPUInstanceCountBufferOffset; }
	FORCEINLINE FRWBuffer& GetGPUIDToIndexTable() { return GPUIDToIndexTable; }

	FORCEINLINE void SetGPUInstanceCountBufferOffset(uint32 Offset) { GPUInstanceCountBufferOffset = Offset; }

	FORCEINLINE int32 GetSafeComponentBufferSize() const { return GetSafeComponentBufferSize(GetNumInstancesAllocated()); }
	FORCEINLINE uint32 GetFloatStride() const { return FloatStride; }
	FORCEINLINE uint32 GetInt32Stride() const { return Int32Stride; }
	FORCEINLINE uint32 GetHalfStride() const { return HalfStride; }

	FORCEINLINE uint32 GetIDAcquireTag() const { return IDAcquireTag; }
	FORCEINLINE void SetIDAcquireTag(uint32 InTag) { IDAcquireTag = InTag; }

	FORCEINLINE FNiagaraDataSet* GetOwner()const { return Owner; }

	int32 TransferInstance(FNiagaraDataBuffer& SourceBuffer, int32 InstanceIndex, bool bRemoveFromSource=true);

	bool CheckForNaNs()const;

	FORCEINLINE TArray<int32>& GetIDTable() { return IDToIndexTable; }
	FORCEINLINE const TArray<int32>& GetIDTable() const { return IDToIndexTable; }

	FORCEINLINE void ClearGPUInstanceCount() { GPUInstanceCountBufferOffset = INDEX_NONE; }

	void BuildRegisterTable();

private:
	FORCEINLINE void CheckUsage(bool bReadOnly)const;

	FORCEINLINE int32 GetSafeComponentBufferSize(int32 RequiredSize) const
	{
		//Round up to VECTOR_WIDTH_BYTES.
		//Both aligns the component buffers to the vector width but also ensures their ops cannot stomp over one another.		
		return Align(RequiredSize, VECTOR_WIDTH_BYTES) + VECTOR_WIDTH_BYTES;
	}

	/** Back ptr to our owning data set. Used to access layout info for the buffer. */
	FNiagaraDataSet* Owner;

	//////////////////////////////////////////////////////////////////////////
	//CPU Data
	/** Float components of simulation data. */
	TArray<uint8> FloatData;
	/** Int32 components of simulation data. */
	TArray<uint8> Int32Data;
	/** Half components of simulation data. */
	TArray<uint8> HalfData;

	/** Table of IDs to real buffer indices. */
	TArray<int32> IDToIndexTable;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// GPU Data
	/** Location in the frame where GPU data will be ready, for CPU this is always the first group, for GPU is depends on the features used as to which phase. */
	ENiagaraGpuComputeTickStage::Type GPUDataReadyStage = ENiagaraGpuComputeTickStage::First;
	/** The buffer offset where the instance count is accumulated. */
	uint32 GPUInstanceCountBufferOffset;
	/** GPU Buffer containing floating point values for GPU simulations. */
	FRWBuffer GPUBufferFloat;
	/** GPU Buffer containing integer values for GPU simulations. */
	FRWBuffer GPUBufferInt;
	/** GPU table which maps particle ID to index. */
	FRWBuffer GPUIDToIndexTable;
	/** GPU Buffer containing half values for GPU simulations. */
	FRWBuffer GPUBufferHalf;
#if NIAGARA_MEMORY_TRACKING
	int32 AllocationSizeBytes = 0;
#endif
	//////////////////////////////////////////////////////////////////////////

	/** Number of instances in data. */
	uint32 NumInstances;
	/** Number of instances the buffer has been allocated for. */
	uint32 NumInstancesAllocated;
	/** Stride between components in the float buffer. */
	uint32 FloatStride;
	/** Stride between components in the int32 buffer. */
	uint32 Int32Stride;
	/** Stride between components in the half buffer. */
	uint32 HalfStride;
	/** Number of instances spawned in the last tick. */
	uint32 NumSpawnedInstances;
	/** ID acquire tag used in the last tick. */
	uint32 IDAcquireTag;

	/** Table containing current base locations for all registers in this dataset. */
	TArray<uint8*> RegisterTable;//TODO: Should make inline? Feels like a useful size to keep local would be too big.
	RegisterTypeOffsetType RegisterTypeOffsets;
};

//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct NIAGARA_API FNiagaraDataSetCompiledData
{
	GENERATED_BODY()

	/** Variables in the data set. */
	UPROPERTY()
	TArray<FNiagaraVariable> Variables;

	/** Data describing the layout of variable data. */
	UPROPERTY()
	TArray<FNiagaraVariableLayoutInfo> VariableLayouts;

	/** Unique ID for this DataSet. Used to allow referencing from other emitters and Systems. */
	UPROPERTY()
	FNiagaraDataSetID ID;

	/** Total number of components of each type in the data set. */
	UPROPERTY()
	uint32 TotalFloatComponents;

	UPROPERTY()
	uint32 TotalInt32Components;

	UPROPERTY()
	uint32 TotalHalfComponents;

	/** Whether or not this dataset require persistent IDs. */
	UPROPERTY()
	uint32 bRequiresPersistentIDs : 1;

	/** Sim target this DataSet is targeting (CPU/GPU). */
	UPROPERTY()
	ENiagaraSimTarget SimTarget;

	FNiagaraDataSetCompiledData();
	const FNiagaraVariableLayoutInfo* FindVariableLayoutInfo(const FNiagaraVariableBase& VariableDef) const;
	void BuildLayout();
	void Empty();

	static FNiagaraDataSetCompiledData DummyCompiledData;
};


//////////////////////////////////////////////////////////////////////////

/**
General storage class for all per instance simulation data in Niagara.
*/
class NIAGARA_API FNiagaraDataSet
{
	friend FNiagaraDataBuffer;
	friend class FNiagaraGpuComputeDispatch;

public:

	FNiagaraDataSet();
	~FNiagaraDataSet();
	FNiagaraDataSet& operator=(const FNiagaraDataSet&) = delete;

	/** Initialize the data set with the compiled data */
	void Init(const FNiagaraDataSetCompiledData* InDataSetCompiledData);

	/** Resets current data but leaves variable/layout information etc intact. */
	void ResetBuffers();

	/** Begins a new simulation pass and grabs a destination buffer. Returns the new destination data buffer. */
	FNiagaraDataBuffer& BeginSimulate(bool bResetDestinationData = true);

	/** Ends a simulation pass and sets the current simulation state. */
	void EndSimulate(bool SetCurrentData = true);

	/** Set current data directly, you can not call this while inside a BeginSimulate block */
	void SetCurrentData(FNiagaraDataBuffer* CurrentData);

	/** Allocates space for NumInstances in the current destination buffer. */
	void Allocate(int32 NumInstances, bool bMaintainExisting = false);

	/** Returns size in bytes for all data buffers currently allocated by this dataset. */
	int64 GetSizeBytes() const;

	FORCEINLINE bool IsInitialized() const { return bInitialized; }
	FORCEINLINE ENiagaraSimTarget GetSimTarget() const { return CompiledData->SimTarget; }
	FORCEINLINE FNiagaraDataSetID GetID() const { return CompiledData->ID; }	
	FORCEINLINE bool RequiresPersistentIDs() const { return CompiledData->bRequiresPersistentIDs; }

	FORCEINLINE TArray<int32>& GetFreeIDTable() { return FreeIDsTable; }
	FORCEINLINE TArray<int32>& GetSpawnedIDsTable() { return SpawnedIDsTable; }
	FORCEINLINE const TArray<int32>& GetFreeIDTable() const { return FreeIDsTable; }
	FORCEINLINE const TArray<int32>& GetSpawnedIDsTable() const { return SpawnedIDsTable; }
	FORCEINLINE int32& GetNumFreeIDs() { return NumFreeIDs; }
	FORCEINLINE int32& GetMaxUsedID() { return MaxUsedID; }
	FORCEINLINE int32& GetIDAcquireTag() { return IDAcquireTag; }
	FORCEINLINE void SetIDAcquireTag(int32 InTag) { IDAcquireTag = InTag; }
	FORCEINLINE FRWBuffer& GetGPUFreeIDs() { return GPUFreeIDs; }
	FORCEINLINE uint32 GetGPUNumAllocatedIDs() const { return GPUNumAllocatedIDs; }

	FORCEINLINE const TArray<FNiagaraVariable>& GetVariables()const { return CompiledData->Variables; }
	FORCEINLINE uint32 GetNumVariables()const { return CompiledData->Variables.Num(); }
	FORCEINLINE bool HasVariable(const FNiagaraVariable& Var)const { return CompiledData->Variables.Contains(Var); }
	FORCEINLINE bool HasVariable(const FName& Name)const { return CompiledData->Variables.FindByPredicate([&](const FNiagaraVariable& VarInfo) { return VarInfo.GetName() == Name; }) != nullptr; }
	FORCEINLINE uint32 GetNumFloatComponents()const { return CompiledData->TotalFloatComponents; }
	FORCEINLINE uint32 GetNumInt32Components()const { return CompiledData->TotalInt32Components; }
	FORCEINLINE uint32 GetNumHalfComponents()const { return CompiledData->TotalHalfComponents; }

	const TArray<FNiagaraVariableLayoutInfo>& GetVariableLayouts() const { return CompiledData->VariableLayouts; }
	const FNiagaraVariableLayoutInfo* GetVariableLayout(const FNiagaraVariable& Var) const;
	bool GetVariableComponentOffsets(const FNiagaraVariable& Var, int32 &FloatStart, int32 &IntStart, int32& HalfStart) const;

	void CopyTo(FNiagaraDataSet& Other, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE, bool bResetOther=true)const;

	void CopyFromGPUReadback(const float* GPUReadBackFloat, const int* GPUReadBackInt, const FFloat16* GPUReadBackHalf, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE, uint32 FloatStride = 0, uint32 IntStride = 0, uint32 HalfStride = 0);

	void CheckForNaNs() const;

	void Dump(int32 StartIndex, int32 NumInstances, const FString& Label, const FName& SortParameterKey = FName()) const;

	FORCEINLINE bool IsCurrentDataValid()const { return CurrentData != nullptr; }
	FORCEINLINE FNiagaraDataBuffer* GetCurrentData()const {	return CurrentData; }
	FORCEINLINE FNiagaraDataBuffer* GetDestinationData()const { return DestinationData; }

	FORCEINLINE FNiagaraDataBuffer& GetCurrentDataChecked() const
	{
		check(CurrentData);
		return *CurrentData;
	}

	FORCEINLINE FNiagaraDataBuffer& GetDestinationDataChecked() const
	{
		check(DestinationData);
		return *DestinationData;
	}

	void AllocateGPUFreeIDs(uint32 InNumInstances, FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, const TCHAR* DebugSimName);

	void SetMaxInstanceCount(uint32 InMaxInstanceCount) { MaxInstanceCount = InMaxInstanceCount; }
	void SetMaxAllocationCount(uint32 InMaxAllocationCount) { MaxAllocationCount = InMaxAllocationCount; }
	uint32 GetMaxInstanceCount() const { return MaxInstanceCount; }
	uint32 GetMaxAllocationCount() const { return MaxAllocationCount; }

	const FNiagaraDataSetCompiledData& GetCompiledData() const { check(CompiledData.Get() != nullptr); return *CompiledData.Get(); }

	int NumSpawnedIDs;
private:

	void Reset();

	void BuildLayout();

	void ResetBuffersInternal();

	FORCEINLINE void CheckCorrectThread()const
	{
		// In some rare occasions, the render thread might be null, like when offloading work to Lightmass 
		// The final GIsThreadedRendering check keeps us from inadvertently failing when that happens.
#if DO_GUARD_SLOW
		ENiagaraSimTarget SimTarget = GetSimTarget();
		bool CPUSimOK = (SimTarget == ENiagaraSimTarget::CPUSim && !IsInRenderingThread());
		bool GPUSimOK = (SimTarget == ENiagaraSimTarget::GPUComputeSim && IsInRenderingThread());
		checkfSlow(!GIsThreadedRendering || CPUSimOK || GPUSimOK, TEXT("NiagaraDataSet function being called on incorrect thread."));
#endif
	}

	FNiagaraCompiledDataReference<FNiagaraDataSetCompiledData> CompiledData;

	/** Table of free IDs available to allocate next tick. */
	TArray<int32> FreeIDsTable;

	/** Number of free IDs in FreeIDTable. */
	int32 NumFreeIDs;

	/** Max ID seen in last execution. Allows us to shrink the IDTable. */
	int32 MaxUsedID;

	/** Tag to use when new IDs are acquired. Should be unique per tick. */
	int32 IDAcquireTag;

	/** Table of IDs spawned in the last tick (just the index part, the acquire tag is IDAcquireTag for all of them). */
	TArray<int32> SpawnedIDsTable;

	/** GPU buffer of free IDs available on the next tick. */
	FRWBuffer GPUFreeIDs;

	/** NUmber of IDs allocated for the GPU simulation. */
	uint32 GPUNumAllocatedIDs;

	/** Buffer containing the current simulation state. */
	FNiagaraDataBuffer* CurrentData;

	/** Buffer we're currently simulating into. Only valid while we're simulating i.e between PrepareForSimulate and EndSimulate calls.*/
	FNiagaraDataBuffer* DestinationData;

#if NIAGARA_MEMORY_TRACKING
	/** Tracked memory allocations */
	std::atomic<int64> BufferSizeBytes;
#endif

	/**
	Actual data storage. These are passed to and read directly by the RT.
	This is effectively a pool of buffers for this simulation.
	Typically this should only be two or three entries and we search for a free buffer to write into on BeginSimulate();
	We keep track of the Current and Previous buffers which move with each simulate.
	Additional buffers may be in here if they are currently being used by the render thread.
	*/
	TArray<FNiagaraDataBuffer*, TInlineAllocator<2>> Data;

	/* Max instance count is the maximum number of instances we allow. */
	uint32 MaxInstanceCount;
	/* Max allocation couns it eh maximum number of instances we can allocate which can be > MaxInstanceCount due to rounding. */
	uint32 MaxAllocationCount;

	bool bInitialized;
};

/**
Iterator that will pull or push data between a NiagaraDataBuffer and some FNiagaraVariables it contains.
Super slow. Don't use at runtime.
*/
struct FNiagaraDataVariableIterator
{
	FNiagaraDataVariableIterator(const FNiagaraDataBuffer* InData, uint32 StartIdx = 0)
		: Data(InData)
		, CurrIdx(StartIdx)
	{
		Variables = Data->GetOwner()->GetVariables();
	}

	void Get()
	{
		const TArray<FNiagaraVariableLayoutInfo>& VarLayouts = Data->GetOwner()->GetVariableLayouts();
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable& Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo& Layout = VarLayouts[VarIdx];
			Var.AllocateData();
			const uint8* ValuePtr = Var.GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout.FloatComponentStart + CompIdx;
				const float* Src = Data->GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Dst = (float*)(ValuePtr + Layout.LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout.Int32ComponentStart + CompIdx;
				const int32* Src = Data->GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Dst = (int32*)(ValuePtr + Layout.LayoutInfo.Int32ComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout.GetNumHalfComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout.HalfComponentStart + CompIdx;
				const FFloat16* Src = Data->GetInstancePtrHalf(CompBufferOffset, CurrIdx);
				FFloat16* Dst = (FFloat16*)(ValuePtr + Layout.LayoutInfo.HalfComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}

	void Advance() { ++CurrIdx; }
	bool IsValid()const { return Data && CurrIdx < Data->GetNumInstances(); }
	uint32 GetCurrIndex()const { return CurrIdx; }
	const TArray<FNiagaraVariable>& GetVariables()const { return Variables; }
private:

	const FNiagaraDataBuffer* Data;
	TArray<FNiagaraVariable> Variables;

	uint32 CurrIdx;
};

#if WITH_EDITOR
/**
Allows immediate access to GPU data on the CPU, you can then use FNiagaraDataSetAccessor to access the data.
This will make a copy of the GPU data and will stall the CPU until the data is ready from the GPU,
therefore it should only be used for tools / debugging.  For async readback see FNiagaraSystemInstance::RequestCapture.
*/
class NIAGARA_API FScopedNiagaraDataSetGPUReadback
{
public:
	FScopedNiagaraDataSetGPUReadback() {}
	FORCEINLINE ~FScopedNiagaraDataSetGPUReadback()
	{
		if (DataBuffer != nullptr)
		{
			DataBuffer->FloatData.Empty();
			DataBuffer->Int32Data.Empty();
		}
	}

	void ReadbackData(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface, FNiagaraDataSet* InDataSet);
	uint32 GetNumInstances() const { check(DataSet != nullptr); return NumInstances; }

private:
	FNiagaraDataSet*	DataSet = nullptr;
	FNiagaraDataBuffer* DataBuffer = nullptr;
	FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = nullptr;
	uint32				NumInstances = 0;
};
#endif

//////////////////////////////////////////////////////////////////////////

FORCEINLINE void FNiagaraDataBuffer::CheckUsage(bool bReadOnly)const
{
	checkSlow(Owner);

	//We can read on the RT but any modifications must be GT (or GT Task).
	//For GPU sims we must be on the RT.
	checkSlow(
		(Owner->GetSimTarget() == ENiagaraSimTarget::CPUSim && (IsInGameThread() || bReadOnly || !GIsThreadedRendering || !IsInRenderingThread())) ||
		(Owner->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim && IsInRenderingThread())
	);
}

