// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHIPrivate.h"

#if D3D12_RHI_RAYTRACING

#include "RayTracingBuiltInResources.h"

class FD3D12RayTracingPipelineState;
class FD3D12RayTracingShaderTable;

// Built-in local root parameters that are always bound to all hit shaders
struct FHitGroupSystemParameters
{
	D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
	FHitGroupSystemRootConstants RootConstants;
};

class FD3D12RayTracingGeometry : public FRHIRayTracingGeometry, public FD3D12AdapterChild, public FD3D12ShaderResourceRenameListener, public FNoncopyable
{
public:

	FD3D12RayTracingGeometry(FRHICommandListBase& RHICmdList, FD3D12Adapter* Adapter, const FRayTracingGeometryInitializer& Initializer);
	~FD3D12RayTracingGeometry();

	virtual FRayTracingAccelerationStructureAddress GetAccelerationStructureAddress(uint64 GPUIndex) const final override
	{
		checkf(IsInRHIThread() || !IsRunningRHIInSeparateThread(), TEXT("Acceleration structure addresses can only be accessed on RHI timeline due to compaction and defragmentation."));
		checkf(AccelerationStructureBuffers[GPUIndex], 
			TEXT("Trying to get address of acceleration structure '%s' without allocated memory."), *DebugName.ToString());
		return AccelerationStructureBuffers[GPUIndex]->ResourceLocation.GetGPUVirtualAddress();
	}
	virtual void SetInitializer(const FRayTracingGeometryInitializer& Initializer) final override;

	void SetupHitGroupSystemParameters(uint32 InGPUIndex);
	void TransitionBuffers(FD3D12CommandContext& CommandContext);
	void UpdateResidency(FD3D12CommandContext& CommandContext);
	void CompactAccelerationStructure(FD3D12CommandContext& CommandContext, uint32 InGPUIndex, uint64 InSizeAfterCompaction);
	void CreateAccelerationStructureBuildDesc(FD3D12CommandContext& CommandContext, EAccelerationStructureBuildMode BuildMode, D3D12_GPU_VIRTUAL_ADDRESS ScratchBufferAddress,
											D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& OutDesc, TArrayView<D3D12_RAYTRACING_GEOMETRY_DESC>& OutGeometryDescs) const;
	
	// Implement FD3D12ShaderResourceRenameListener interface
	virtual void ResourceRenamed(FRHICommandListBase& RHICmdList, FD3D12BaseShaderResource* InRenamedResource, FD3D12ResourceLocation* InNewResourceLocation) override;

	void RegisterAsRenameListener(uint32 InGPUIndex);
	void UnregisterAsRenameListener(uint32 InGPUIndex);

	void Swap(FD3D12RayTracingGeometry& Other);

	void ReleaseUnderlyingResource();

	bool bIsAccelerationStructureDirty[MAX_NUM_GPUS] = {};
	void SetDirty(FRHIGPUMask GPUMask, bool bState)
	{
		for (uint32 GPUIndex : GPUMask)
		{
			bIsAccelerationStructureDirty[GPUIndex] = bState;
		}
	}
	bool IsDirty(uint32 GPUIndex) const
	{
		return bIsAccelerationStructureDirty[GPUIndex];
	}
	bool BuffersValid(uint32 GPUIndex) const;

	using FRHIRayTracingGeometry::Initializer;
	using FRHIRayTracingGeometry::SizeInfo;

	static constexpr uint32 IndicesPerPrimitive = 3; // Triangle geometry only

	static FBufferRHIRef NullTransformBuffer; // Null transform for hidden sections

	TRefCountPtr<FD3D12Buffer> AccelerationStructureBuffers[MAX_NUM_GPUS];

	bool bRegisteredAsRenameListener[MAX_NUM_GPUS];
	bool bHasPendingCompactionRequests[MAX_NUM_GPUS];

	// Hit shader parameters per geometry segment
	TArray<FHitGroupSystemParameters> HitGroupSystemParameters[MAX_NUM_GPUS];

	FDebugName DebugName;
	FName OwnerName;		// Store the path name of the owner object for resource tracking

	// Array of geometry descriptions, one per segment (single-segment geometry is a common case).
	// Only references CPU-accessible structures (no GPU resources).
	// Used as a template for BuildAccelerationStructure() later.
	TArray<D3D12_RAYTRACING_GEOMETRY_DESC, TInlineAllocator<1>> GeometryDescs;

	uint64 AccelerationStructureCompactedSize = 0;
};

class FD3D12RayTracingScene : public FRHIRayTracingScene, public FD3D12AdapterChild, public FNoncopyable
{
public:

	// Ray tracing shader bindings can be processed in parallel.
	// Each concurrent worker gets its own dedicated descriptor cache instance to avoid contention or locking.
	// Scaling beyond 5 total threads does not yield any speedup in practice.
	static constexpr uint32 MaxBindingWorkers = 5; // RHI thread + 4 parallel workers.

	FD3D12RayTracingScene(FD3D12Adapter* Adapter, FRayTracingSceneInitializer2 Initializer);
	~FD3D12RayTracingScene();

	const FRayTracingSceneInitializer2& GetInitializer() const override final { return Initializer; }
	uint32 GetLayerBufferOffset(uint32 LayerIndex) const override final { return Layers[LayerIndex].BufferOffset; }

	void BindBuffer(FRHIBuffer* Buffer, uint32 BufferOffset);
	void ReleaseBuffer();

	void BuildAccelerationStructure(FD3D12CommandContext& CommandContext,
		FD3D12Buffer* ScratchBuffer, uint32 ScratchBufferOffset,
		FD3D12Buffer* InstanceBuffer, uint32 InstanceBufferOffset
	);

	struct FLayerData
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BuildInputs = {};
		FRayTracingAccelerationStructureSize SizeInfo = {};
		uint32 BufferOffset;
		uint32 ScratchBufferOffset;
	};

	TArray<FLayerData> Layers;

	TRefCountPtr<FD3D12Buffer> AccelerationStructureBuffers[MAX_NUM_GPUS];
	uint32 BufferOffset = 0;

	const FRayTracingSceneInitializer2 Initializer;

	// Scene keeps track of child acceleration structure buffers to ensure
	// they are resident when any ray tracing work is dispatched.
	TArray<FD3D12ResidencyHandle*> GeometryResidencyHandles[MAX_NUM_GPUS];

	void UpdateResidency(FD3D12CommandContext& CommandContext);

	uint32 GetHitRecordBaseIndex(uint32 InstanceIndex, uint32 SegmentIndex) const { return (Initializer.SegmentPrefixSum[InstanceIndex] + SegmentIndex) * Initializer.ShaderSlotsPerGeometrySegment; }

	// Array of hit group parameters per geometry segment across all scene instance geometries.
	// Accessed as HitGroupSystemParametersCache[SegmentPrefixSum[InstanceIndex] + SegmentIndex].
	// Only used for GPU 0 (secondary GPUs take the slow path).
	TArray<FHitGroupSystemParameters> HitGroupSystemParametersCache;

	// #dxr_todo UE-68230: shader tables should be explicitly registered and unregistered with the scene
	FD3D12RayTracingShaderTable* FindOrCreateShaderTable(const FD3D12RayTracingPipelineState* Pipeline, FD3D12Device* Device);
	FD3D12RayTracingShaderTable* FindExistingShaderTable(const FD3D12RayTracingPipelineState* Pipeline, FD3D12Device* Device) const;

	TMap<const FD3D12RayTracingPipelineState*, FD3D12RayTracingShaderTable*> ShaderTables[MAX_NUM_GPUS];

	uint64 LastCommandListID = 0;

	bool bBuilt = false;
};

// Manages all the pending BLAS compaction requests
class FD3D12RayTracingCompactionRequestHandler : FD3D12DeviceChild
{
public:

	UE_NONCOPYABLE(FD3D12RayTracingCompactionRequestHandler)

	FD3D12RayTracingCompactionRequestHandler(FD3D12Device* Device);
	~FD3D12RayTracingCompactionRequestHandler()
	{
		check(PendingRequests.IsEmpty());
	}

	void RequestCompact(FD3D12RayTracingGeometry* InRTGeometry);
	bool ReleaseRequest(FD3D12RayTracingGeometry* InRTGeometry);

	void Update(FD3D12CommandContext& InCommandContext);

private:

	FCriticalSection CS;
	TArray<FD3D12RayTracingGeometry*> PendingRequests;
	TArray<FD3D12RayTracingGeometry*> ActiveRequests;
	TArray<D3D12_GPU_VIRTUAL_ADDRESS> ActiveBLASGPUAddresses;

	TRefCountPtr<FD3D12Buffer> PostBuildInfoBuffer;
	FStagingBufferRHIRef PostBuildInfoStagingBuffer;
	FD3D12SyncPointRef PostBuildInfoBufferReadbackSyncPoint;
};

#endif // D3D12_RHI_RAYTRACING
