// Copyright Epic Games, Inc. All Rights Reserved..

#pragma once

#include "VulkanRHIPrivate.h"

#if VULKAN_RHI_RAYTRACING

#include "RayTracingBuiltInResources.h"

class FVulkanCommandListContext;
class FVulkanResourceMultiBuffer;

class FVulkanRayTracingPlatform
{
public:
	static bool CheckVulkanInstanceFunctions(VkInstance inInstance);
};


// Built-in local root parameters that are always bound to all hit shaders
// :todo-jn: NOTE: Keep in sync with VulkanCommon.ush decl until it's put in a common header
struct FVulkanHitGroupSystemParameters
{
	FHitGroupSystemRootConstants RootConstants;

	uint32 BindlessHitGroupSystemIndexBuffer;
	uint32 BindlessHitGroupSystemVertexBuffer;

	uint32 BindlessUniformBuffers[16];
};


struct FVkRtTLASBuildData
{
	FVkRtTLASBuildData()
	{
		ZeroVulkanStruct(Geometry, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
		ZeroVulkanStruct(GeometryInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
		ZeroVulkanStruct(SizesInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);
	}

	VkAccelerationStructureGeometryKHR Geometry;
	VkAccelerationStructureBuildGeometryInfoKHR GeometryInfo;
	VkAccelerationStructureBuildSizesInfoKHR SizesInfo;
};

struct FVkRtBLASBuildData
{
	FVkRtBLASBuildData()
	{
		ZeroVulkanStruct(GeometryInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
		ZeroVulkanStruct(SizesInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);
	}

	TArray<VkAccelerationStructureGeometryKHR, TInlineAllocator<1>> Segments;
	TArray<VkAccelerationStructureBuildRangeInfoKHR, TInlineAllocator<1>> Ranges;
	VkAccelerationStructureBuildGeometryInfoKHR GeometryInfo;
	VkAccelerationStructureBuildSizesInfoKHR SizesInfo;
};

class FVulkanRayTracingShaderTable : VulkanRHI::FDeviceChild
{
public:
	FVulkanRayTracingShaderTable(FVulkanDevice* Device);
	~FVulkanRayTracingShaderTable();

	void Init(const FVulkanRayTracingScene* Scene, const FVulkanRayTracingPipelineState* Pipeline);

	const VkStridedDeviceAddressRegionKHR* GetRegion(EShaderFrequency Frequency);

	void SetSlot(EShaderFrequency Frequency, uint32 DstSlot, uint32 SrcHandleIndex, TConstArrayView<uint8> SrcHandleData);

	template <typename T>
	void SetLocalShaderParameters(EShaderFrequency Frequency, uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const T& Parameters)
	{
		SetLocalShaderParameters(Frequency, RecordIndex, InOffsetWithinRootSignature, &Parameters, sizeof(Parameters));
	}

	void SetLocalShaderParameters(EShaderFrequency Frequency, uint32 RecordIndex, uint32 OffsetWithinRecord, const void* InData, uint32 InDataSize);

	void Commit(FVulkanCommandListContext& Context);

	void AddUBRef(FRHIUniformBuffer* UB)
	{
		ReferencedUniformBuffers.AddUnique(UB);
	}

	TArrayView<TRefCountPtr<FRHIUniformBuffer>> GetUBRefs()
	{
		return ReferencedUniformBuffers;
	}

private:

	struct FVulkanShaderTableAllocation
	{
		FVulkanShaderTableAllocation()
		{
			FMemory::Memzero(Region);
		}

		uint32 HandleCount = 0;
		bool bUseLocalRecord = false;

		// Host memory copy
		TArray<uint8> HostBuffer;

		// GPU Local memory copy
		VkBuffer LocalBuffer = VK_NULL_HANDLE;
		VulkanRHI::FVulkanAllocation LocalAllocation;
		VkStridedDeviceAddressRegionKHR Region;

		bool bIsDirty = true;
	};

	FVulkanShaderTableAllocation& GetAlloc(EShaderFrequency Frequency);
	static void ReleaseLocalBuffer(FVulkanDevice* Device, FVulkanShaderTableAllocation& Alloc);

	FVulkanShaderTableAllocation Raygen;
	FVulkanShaderTableAllocation Miss;
	FVulkanShaderTableAllocation HitGroup;
	FVulkanShaderTableAllocation Callable;

	TArray<TRefCountPtr<FRHIUniformBuffer>> ReferencedUniformBuffers;

	// Convenience
	const uint32 HandleSize;
	const uint32 HandleSizeAligned;
};

class FVulkanRayTracingGeometry : public FRHIRayTracingGeometry
{
public:
	static constexpr uint32 IndicesPerPrimitive = 3; // Only triangle meshes are supported

	FVulkanRayTracingGeometry(ENoInit);
	FVulkanRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer, FVulkanDevice* InDevice);
	~FVulkanRayTracingGeometry();

	virtual FRayTracingAccelerationStructureAddress GetAccelerationStructureAddress(uint64 GPUIndex) const final override { return Address; }	
	virtual void SetInitializer(const FRayTracingGeometryInitializer& Initializer) final override;

	void Swap(FVulkanRayTracingGeometry& Other);

	using FRHIRayTracingGeometry::Initializer;
	using FRHIRayTracingGeometry::SizeInfo;
	
	void RemoveCompactionRequest();
	void CompactAccelerationStructure(FVulkanCmdBuffer& CmdBuffer, uint64 InSizeAfterCompaction);

	void SetupHitGroupSystemParameters();
	void ReleaseBindlessHandles();

	VkAccelerationStructureKHR Handle = VK_NULL_HANDLE;
	VkDeviceAddress Address = 0;
	TRefCountPtr<FVulkanResourceMultiBuffer> AccelerationStructureBuffer;
	bool bHasPendingCompactionRequests = false;
	uint64 AccelerationStructureCompactedSize = 0;

	FVulkanDevice* const Device = nullptr;

	TArray<FVulkanHitGroupSystemParameters> HitGroupSystemParameters;
	TArray<FRHIDescriptorHandle> HitGroupSystemVertexViews;  // :todo-jn: use views?
	FRHIDescriptorHandle HitGroupSystemIndexView;

	FDebugName DebugName;
	FName OwnerName;		// Store the path name of the owner object for resource tracking
};

class FVulkanRayTracingScene : public FRHIRayTracingScene, public VulkanRHI::FDeviceChild
{
public:
	// Ray tracing shader bindings can be processed in parallel.
	// Each concurrent worker gets its own dedicated descriptor cache instance to avoid contention or locking.
	// Scaling beyond 5 total threads does not yield any speedup in practice (RHI thread + 4 parallel workers).
	static constexpr uint32 MaxBindingWorkers = 1; // :todo-jn:

	FVulkanRayTracingScene(FRayTracingSceneInitializer2 Initializer, FVulkanDevice* InDevice);
	~FVulkanRayTracingScene();

	const FRayTracingSceneInitializer2& GetInitializer() const override final { return Initializer; }
	uint32 GetLayerBufferOffset(uint32 LayerIndex) const override final { return Layers[LayerIndex].BufferOffset; }

	void BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset);
	void BuildAccelerationStructure(
		FVulkanCommandListContext& CommandContext, 
		FVulkanResourceMultiBuffer* ScratchBuffer, uint32 ScratchOffset, 
		FVulkanResourceMultiBuffer* InstanceBuffer, uint32 InstanceOffset);

	virtual FRHIShaderResourceView* GetOrCreateMetadataBufferSRV(FRHICommandListImmediate& RHICmdList) override final
	{
		if (!PerInstanceGeometryParameterSRV.IsValid())
		{
			PerInstanceGeometryParameterSRV = RHICmdList.CreateShaderResourceView(PerInstanceGeometryParameterBuffer, FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Structured));
		}

		return PerInstanceGeometryParameterSRV.GetReference();
	}

	FVulkanRayTracingShaderTable* FindOrCreateShaderTable(const FVulkanRayTracingPipelineState* Pipeline);

	inline uint32 GetHitRecordBaseIndex(uint32 InstanceIndex, uint32 SegmentIndex) const 
	{
		return (Initializer.SegmentPrefixSum[InstanceIndex] + SegmentIndex) * Initializer.ShaderSlotsPerGeometrySegment;
	}

	inline bool IsBuilt() const
	{
		return bBuilt;
	}

private:
	const FRayTracingSceneInitializer2 Initializer;

	// Native TLAS handles are owned by SRV objects in Vulkan RHI.
	// D3D12 and other RHIs allow creating TLAS SRVs from any GPU address at any point
	// and do not require them for operations such as build or update.
	// FVulkanRayTracingScene can't own the VkAccelerationStructureKHR directly because
	// we allow TLAS memory to be allocated using transient resource allocator and 
	// the lifetime of the scene object may be different from the lifetime of the buffer.
	// Many VkAccelerationStructureKHR-s may be created, pointing at the same buffer.

	struct FLayerData
	{
		TUniquePtr<FVulkanView> View;
		uint32 BufferOffset;
		uint32 ScratchBufferOffset;
	};

	TArray<FLayerData> Layers;
	
	TRefCountPtr<FVulkanResourceMultiBuffer> AccelerationStructureBuffer;

	// Buffer that contains per-instance index and vertex buffer binding data
	TRefCountPtr<FVulkanResourceMultiBuffer> PerInstanceGeometryParameterBuffer;
	FShaderResourceViewRHIRef PerInstanceGeometryParameterSRV;
	
	TMap<const FVulkanRayTracingPipelineState*, FVulkanRayTracingShaderTable*> ShaderTables;

	void BuildPerInstanceGeometryParameterBuffer(FVulkanCommandListContext& CommandContext);

	bool bBuilt = false;
};


class FVulkanRayTracingPipelineState : public FRHIRayTracingPipelineState, public VulkanRHI::FDeviceChild
{
public:

	UE_NONCOPYABLE(FVulkanRayTracingPipelineState);
	FVulkanRayTracingPipelineState(FVulkanDevice* const InDevice, const FRayTracingPipelineStateInitializer& Initializer);
	~FVulkanRayTracingPipelineState();

	inline VkPipeline GetPipeline() const
	{
		return Pipeline;
	}

	int32 GetShaderIndex(const FVulkanRayTracingShader* Shader) const;
	const FVulkanRayTracingShader* GetShader(EShaderFrequency Frequency, int32 ShaderIndex) const;
	const TArray<uint8>& GetShaderHandles(EShaderFrequency Frequency) const;

private:

	struct ShaderData
	{
		TArray<TRefCountPtr<FVulkanRayTracingShader>> Shaders;
		TArray<uint8> ShaderHandles;
	};

	const ShaderData& GetShaderData(EShaderFrequency Frequency) const;

	ShaderData RayGen;
	ShaderData Miss;
	ShaderData HitGroup;
	ShaderData Callable;

	VkPipeline Pipeline = VK_NULL_HANDLE;

public:
	bool bAllowHitGroupIndexing = true;

	friend FVulkanCommandListContext;
	friend FVulkanRayTracingScene;
};

class FVulkanRayTracingCompactedSizeQueryPool : public FVulkanQueryPool
{
public:
	FVulkanRayTracingCompactedSizeQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries)
		: FVulkanQueryPool(InDevice, nullptr, InMaxQueries, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, false)
	{}

	void EndBatch(FVulkanCmdBuffer* InCmdBuffer);
	bool TryGetResults(uint32 NumResults);
	void Reset(FVulkanCmdBuffer* InCmdBuffer);

	FVulkanCmdBuffer* CmdBuffer = nullptr;
	uint64 FenceSignaledCounter = 0;
};

// Manages all the pending BLAS compaction requests
class FVulkanRayTracingCompactionRequestHandler : public VulkanRHI::FDeviceChild
{
public:
	UE_NONCOPYABLE(FVulkanRayTracingCompactionRequestHandler)

	FVulkanRayTracingCompactionRequestHandler(FVulkanDevice* const InDevice);
	~FVulkanRayTracingCompactionRequestHandler()
	{
		check(PendingRequests.IsEmpty());
		delete QueryPool;
	}

	void RequestCompact(FVulkanRayTracingGeometry* InRTGeometry);
	bool ReleaseRequest(FVulkanRayTracingGeometry* InRTGeometry);

	void Update(FVulkanCommandListContext& InCommandContext);

private:

	FCriticalSection CS;
	TArray<FVulkanRayTracingGeometry*> PendingRequests;
	TArray<FVulkanRayTracingGeometry*> ActiveRequests;
	TArray<VkAccelerationStructureKHR> ActiveBLASes;

	FVulkanRayTracingCompactedSizeQueryPool* QueryPool = nullptr;
};

#endif // VULKAN_RHI_RAYTRACING
