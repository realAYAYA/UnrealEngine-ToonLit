// Copyright Epic Games, Inc. All Rights Reserved..

#pragma once

#include "VulkanRHIPrivate.h"

#if VULKAN_RHI_RAYTRACING

class FVulkanCommandListContext;
class FVulkanResourceMultiBuffer;
class FVulkanRayTracingLayout;

class FVulkanRayTracingPlatform
{
public:
	static bool CheckVulkanInstanceFunctions(VkInstance inInstance);
};

struct FVkRtAllocation
{
	VkDevice Device = VK_NULL_HANDLE;
	VkDeviceMemory Memory = VK_NULL_HANDLE;
	VkBuffer Buffer = VK_NULL_HANDLE;
};

class FVulkanRayTracingAllocator
{
public:
	static void Allocate(FVulkanDevice* Device, VkDeviceSize Size, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryFlags, FVkRtAllocation& Result);
	static void Free(FVkRtAllocation& Allocation);
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

class FVulkanRayTracingGeometry : public FRHIRayTracingGeometry
{
public:
	FVulkanRayTracingGeometry(ENoInit);
	FVulkanRayTracingGeometry(const FRayTracingGeometryInitializer& Initializer, FVulkanDevice* InDevice);
	~FVulkanRayTracingGeometry();

	virtual FRayTracingAccelerationStructureAddress GetAccelerationStructureAddress(uint64 GPUIndex) const final override { return Address; }	
	virtual void SetInitializer(const FRayTracingGeometryInitializer& Initializer) final override;

	void Swap(FVulkanRayTracingGeometry& Other);

	using FRHIRayTracingGeometry::Initializer;
	using FRHIRayTracingGeometry::SizeInfo;
	
	void RemoveCompactionRequest();
	void CompactAccelerationStructure(FVulkanCmdBuffer& CmdBuffer, uint64 InSizeAfterCompaction);

	VkAccelerationStructureKHR Handle = VK_NULL_HANDLE;
	VkDeviceAddress Address = 0;
	TRefCountPtr<FVulkanResourceMultiBuffer> AccelerationStructureBuffer;
	bool bHasPendingCompactionRequests = false;
	uint64 AccelerationStructureCompactedSize = 0;
private:
	FVulkanDevice* const Device = nullptr;
};

class FVulkanRayTracingScene : public FRHIRayTracingScene
{
public:
	FVulkanRayTracingScene(FRayTracingSceneInitializer2 Initializer, FVulkanDevice* InDevice);
	~FVulkanRayTracingScene();

	const FRayTracingSceneInitializer2& GetInitializer() const override final { return Initializer; }
	uint32 GetLayerBufferOffset(uint32 LayerIndex) const override final { return Layers[LayerIndex].BufferOffset; }

	void BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset);
	void BuildAccelerationStructure(
		FVulkanCommandListContext& CommandContext, 
		FVulkanResourceMultiBuffer* ScratchBuffer, uint32 ScratchOffset, 
		FVulkanResourceMultiBuffer* InstanceBuffer, uint32 InstanceOffset);

	virtual FRHIShaderResourceView* GetMetadataBufferSRV() const override final
	{
		return PerInstanceGeometryParameterSRV.GetReference();
	}

private:
	FVulkanDevice* const Device = nullptr;

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
		TRefCountPtr<FVulkanShaderResourceView> ShaderResourceView;
		uint32 BufferOffset;
		uint32 ScratchBufferOffset;
	};

	TArray<FLayerData> Layers;
	
	TRefCountPtr<FVulkanResourceMultiBuffer> AccelerationStructureBuffer;

	// Buffer that contains per-instance index and vertex buffer binding data
	TRefCountPtr<FVulkanResourceMultiBuffer> PerInstanceGeometryParameterBuffer;
	TRefCountPtr<FVulkanShaderResourceView> PerInstanceGeometryParameterSRV;
	void BuildPerInstanceGeometryParameterBuffer(FVulkanCommandListContext& CommandContext);
};

class FVulkanRayTracingPipelineState : public FRHIRayTracingPipelineState
{
public:

	UE_NONCOPYABLE(FVulkanRayTracingPipelineState);
	FVulkanRayTracingPipelineState(FVulkanDevice* const InDevice, const FRayTracingPipelineStateInitializer& Initializer);
	~FVulkanRayTracingPipelineState();

private:

	FVulkanRayTracingLayout* Layout = nullptr;
	VkPipeline Pipeline = VK_NULL_HANDLE;
	FVkRtAllocation RayGenShaderBindingTable;
	FVkRtAllocation MissShaderBindingTable;
	FVkRtAllocation HitShaderBindingTable;
};

class FVulkanBasicRaytracingPipeline
{
public:

	UE_NONCOPYABLE(FVulkanBasicRaytracingPipeline);
	FVulkanBasicRaytracingPipeline(FVulkanDevice* const InDevice);
	~FVulkanBasicRaytracingPipeline();

private:

	FVulkanRayTracingPipelineState* Occlusion = nullptr;
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
