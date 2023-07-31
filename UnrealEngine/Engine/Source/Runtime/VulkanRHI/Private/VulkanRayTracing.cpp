// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanRayTracing.h"

#if VULKAN_RHI_RAYTRACING

#include "VulkanContext.h"
#include "VulkanDescriptorSets.h"
#include "BuiltInRayTracingShaders.h"
#include "Experimental/Containers/SherwoodHashTable.h"
#include "Async/ParallelFor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static int32 GVulkanRayTracingAllowCompaction = 1;
static FAutoConsoleVariableRef CVarVulkanRayTracingAllowCompaction(
	TEXT("r.Vulkan.RayTracing.AllowCompaction"),
	GVulkanRayTracingAllowCompaction,
	TEXT("Whether to automatically perform compaction for static acceleration structures to save GPU memory. (default = 1)\n"),
	ECVF_ReadOnly
);

static int32 GVulkanRayTracingMaxBatchedCompaction = 64;
static FAutoConsoleVariableRef CVarVulkanRayTracingMaxBatchedCompaction(
	TEXT("r.Vulkan.RayTracing.MaxBatchedCompaction"),
	GVulkanRayTracingMaxBatchedCompaction,
	TEXT("Maximum of amount of compaction requests and rebuilds per frame. (default = 64)\n"),
	ECVF_ReadOnly
);

#if PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion
#endif // PLATFORM_WINDOWS
bool FVulkanRayTracingPlatform::CheckVulkanInstanceFunctions(VkInstance inInstance)
{
	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
#if UE_BUILD_DEBUG
	ENUM_VK_ENTRYPOINTS_RAYTRACING(CHECK_VK_ENTRYPOINTS);
#endif
	return bFoundAllEntryPoints;
}
#if PLATFORM_WINDOWS
#pragma warning(pop) // restore 4191
#endif

static VkDeviceAddress GetDeviceAddress(VkDevice Device, VkBuffer Buffer)
{
	VkBufferDeviceAddressInfoKHR DeviceAddressInfo;
	ZeroVulkanStruct(DeviceAddressInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
	DeviceAddressInfo.buffer = Buffer;
	return VulkanDynamicAPI::vkGetBufferDeviceAddressKHR(Device, &DeviceAddressInfo);
}

VkDeviceAddress FVulkanResourceMultiBuffer::GetDeviceAddress() const
{
	return ::GetDeviceAddress(Device->GetInstanceHandle(), GetHandle()) + GetOffset();
}

// Temporary brute force allocation helper, this should be handled by the memory sub-allocator
static uint32 FindMemoryType(VkPhysicalDevice Gpu, uint32 Filter, VkMemoryPropertyFlags RequestedProperties)
{
	VkPhysicalDeviceMemoryProperties Properties = {};
	VulkanRHI::vkGetPhysicalDeviceMemoryProperties(Gpu, &Properties);

	uint32 Result = UINT32_MAX;
	for (uint32 i = 0; i < Properties.memoryTypeCount; ++i)
	{
		const bool bTypeFilter = Filter & (1 << i);
		const bool bPropFilter = (Properties.memoryTypes[i].propertyFlags & RequestedProperties) == RequestedProperties;
		if (bTypeFilter && bPropFilter)
		{
			Result = i;
			break;
		}
	}

	check(Result < UINT32_MAX);
	return Result;
}

// Temporary brute force allocation
void FVulkanRayTracingAllocator::Allocate(FVulkanDevice* Device, VkDeviceSize Size, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryFlags, FVkRtAllocation& Result)
{
	VkMemoryRequirements MemoryRequirements;
	Result.Buffer = VulkanRHI::CreateBuffer(Device, Size, UsageFlags, MemoryRequirements);

	VkDevice DeviceHandle = Device->GetInstanceHandle();
	VkPhysicalDevice Gpu = Device->GetPhysicalHandle();

	VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo;
	ZeroVulkanStruct(MemoryAllocateFlagsInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
	MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo MemoryAllocateInfo;
	ZeroVulkanStruct(MemoryAllocateInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
	MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
	MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
	MemoryAllocateInfo.memoryTypeIndex = FindMemoryType(Gpu, MemoryRequirements.memoryTypeBits, MemoryFlags);
	VERIFYVULKANRESULT(VulkanRHI::vkAllocateMemory(DeviceHandle, &MemoryAllocateInfo, VULKAN_CPU_ALLOCATOR, &Result.Memory));
	VERIFYVULKANRESULT(VulkanRHI::vkBindBufferMemory(DeviceHandle, Result.Buffer, Result.Memory, 0));

	Result.Device = DeviceHandle;
}

// Temporary brute force deallocation
void FVulkanRayTracingAllocator::Free(FVkRtAllocation& Allocation)
{
	if (Allocation.Buffer != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyBuffer(Allocation.Device, Allocation.Buffer, VULKAN_CPU_ALLOCATOR);
		Allocation.Buffer = VK_NULL_HANDLE;
	}
	if (Allocation.Memory != VK_NULL_HANDLE)
	{
		VulkanRHI::vkFreeMemory(Allocation.Device, Allocation.Memory, VULKAN_CPU_ALLOCATOR);
		Allocation.Memory = VK_NULL_HANDLE;
	}
}

static void AddAccelerationStructureBuildBarrier(VkCommandBuffer CommandBuffer)
{
	VkMemoryBarrier Barrier;
	ZeroVulkanStruct(Barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
	Barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	Barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	VulkanRHI::vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &Barrier, 0, nullptr, 0, nullptr);
}

static bool ShouldCompactAfterBuild(ERayTracingAccelerationStructureFlags BuildFlags)
{
	return EnumHasAllFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction | ERayTracingAccelerationStructureFlags::FastTrace)
		&& !EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
}

static ERayTracingAccelerationStructureFlags GetRayTracingAccelerationStructureBuildFlags(const FRayTracingGeometryInitializer& Initializer)
{
	ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::None;

	if (Initializer.bFastBuild)
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastBuild;
	}
	else
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace;
	}

	if (Initializer.bAllowUpdate)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
	}

	if (!Initializer.bFastBuild && !Initializer.bAllowUpdate && Initializer.bAllowCompaction && GVulkanRayTracingAllowCompaction)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction);
	}

	return BuildFlags;
}

static void GetBLASBuildData(
	const VkDevice Device,
	const TArrayView<const FRayTracingGeometrySegment> Segments,
	const ERayTracingGeometryType GeometryType,
	const FBufferRHIRef IndexBufferRHI,
	const uint32 IndexBufferOffset,	
	const uint32 IndexStrideInBytes,
	ERayTracingAccelerationStructureFlags BuildFlags,
	const EAccelerationStructureBuildMode BuildMode,
	FVkRtBLASBuildData& BuildData)
{
	static constexpr uint32 IndicesPerPrimitive = 3; // Only triangle meshes are supported

	FVulkanResourceMultiBuffer* const IndexBuffer = ResourceCast(IndexBufferRHI.GetReference());
	VkDeviceOrHostAddressConstKHR IndexBufferDeviceAddress = {};
	IndexBufferDeviceAddress.deviceAddress = IndexBufferRHI ? IndexBuffer->GetDeviceAddress() + IndexBufferOffset : 0;

	TArray<uint32, TInlineAllocator<1>> PrimitiveCounts;

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FRayTracingGeometrySegment& Segment = Segments[SegmentIndex];

		FVulkanResourceMultiBuffer* const VertexBuffer = ResourceCast(Segment.VertexBuffer.GetReference());

		VkDeviceOrHostAddressConstKHR VertexBufferDeviceAddress = {};
		VertexBufferDeviceAddress.deviceAddress = VertexBuffer->GetDeviceAddress() + Segment.VertexBufferOffset;

		VkAccelerationStructureGeometryKHR SegmentGeometry;
		ZeroVulkanStruct(SegmentGeometry, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);

		if (Segment.bForceOpaque)
		{
			SegmentGeometry.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
		}

		if (!Segment.bAllowDuplicateAnyHitShaderInvocation)
		{
			// Allow only a single any-hit shader invocation per primitive
			SegmentGeometry.flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
		}

		uint32 PrimitiveOffset = 0;
		switch (GeometryType)
		{
			case RTGT_Triangles:
				SegmentGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

				SegmentGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				SegmentGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				SegmentGeometry.geometry.triangles.vertexData = VertexBufferDeviceAddress;
				SegmentGeometry.geometry.triangles.maxVertex = Segment.MaxVertices;
				SegmentGeometry.geometry.triangles.vertexStride = Segment.VertexBufferStride;
				SegmentGeometry.geometry.triangles.indexData = IndexBufferDeviceAddress;

				switch (Segment.VertexBufferElementType)
				{
				case VET_Float3:
				case VET_Float4:
					SegmentGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
					break;
				default:
					checkNoEntry();
					break;
				}

				// No support for segment transform
				SegmentGeometry.geometry.triangles.transformData.deviceAddress = 0;
				SegmentGeometry.geometry.triangles.transformData.hostAddress = nullptr;

				if (IndexBufferRHI)
				{
					SegmentGeometry.geometry.triangles.indexType = (IndexStrideInBytes == 2) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
					// offset in bytes into the index buffer where primitive data for the current segment is defined
					PrimitiveOffset = Segment.FirstPrimitive * IndicesPerPrimitive * IndexStrideInBytes;
				}
				else
				{
					SegmentGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
					// for non-indexed geometry, primitiveOffset is applied when reading from vertex buffer
					PrimitiveOffset = Segment.FirstPrimitive * IndicesPerPrimitive * Segment.VertexBufferStride;
				}

				break;
			case RTGT_Procedural:
				checkf(Segment.VertexBufferStride >= (2 * sizeof(FVector3f)), TEXT("Procedural geometry vertex buffer must contain at least 2xFloat3 that defines 3D bounding boxes of primitives."));
				checkf(Segment.VertexBufferStride % 8 == 0, TEXT("Procedural geometry vertex buffer stride must be a multiple of 8."));

				SegmentGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
				
				SegmentGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
				SegmentGeometry.geometry.aabbs.data = VertexBufferDeviceAddress;
				SegmentGeometry.geometry.aabbs.stride = Segment.VertexBufferStride;

				break;
			default:
				checkf(false, TEXT("Unexpected ray tracing geometry type"));
				break;
		}

		BuildData.Segments.Add(SegmentGeometry);

		VkAccelerationStructureBuildRangeInfoKHR RangeInfo = {};
		RangeInfo.firstVertex = 0;

		// Disabled segments use an empty range. We still build them to keep the sbt valid.
		RangeInfo.primitiveCount = (Segment.bEnabled) ? Segment.NumPrimitives : 0;
		RangeInfo.primitiveOffset = PrimitiveOffset;
		RangeInfo.transformOffset = 0;

		BuildData.Ranges.Add(RangeInfo);

		PrimitiveCounts.Add(Segment.NumPrimitives);
	}

	BuildData.GeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	BuildData.GeometryInfo.flags = (EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastBuild))
		? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR 
		: VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	if (EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate))
	{
		BuildData.GeometryInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	}
	if (EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction))
	{
		BuildData.GeometryInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
	}
	BuildData.GeometryInfo.mode = (BuildMode == EAccelerationStructureBuildMode::Build) ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	BuildData.GeometryInfo.geometryCount = BuildData.Segments.Num();
	BuildData.GeometryInfo.pGeometries = BuildData.Segments.GetData();

	VulkanDynamicAPI::vkGetAccelerationStructureBuildSizesKHR(
		Device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&BuildData.GeometryInfo,
		PrimitiveCounts.GetData(),
		&BuildData.SizesInfo);
}

FVulkanRayTracingGeometry::FVulkanRayTracingGeometry(ENoInit)
{}

FVulkanRayTracingGeometry::FVulkanRayTracingGeometry(const FRayTracingGeometryInitializer& InInitializer, FVulkanDevice* InDevice)
	: FRHIRayTracingGeometry(InInitializer), Device(InDevice)
{
	uint32 IndexBufferStride = 0;
	if (Initializer.IndexBuffer)
	{
		// In case index buffer in initializer is not yet in valid state during streaming we assume the geometry is using UINT32 format.
		IndexBufferStride = Initializer.IndexBuffer->GetSize() > 0
			? Initializer.IndexBuffer->GetStride()
			: 4;
	}

	checkf(!Initializer.IndexBuffer || (IndexBufferStride == 2 || IndexBufferStride == 4), TEXT("Index buffer must be 16 or 32 bit if in use."));

	SizeInfo = RHICalcRayTracingGeometrySize(Initializer);

	// If this RayTracingGeometry going to be used as streaming destination 
	// we don't want to allocate its memory as it will be replaced later by streamed version
	// but we still need correct SizeInfo as it is used to estimate its memory requirements outside of RHI.
	if (Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination)
	{
		return;
	}

	FString DebugNameString = Initializer.DebugName.ToString();
	FRHIResourceCreateInfo BlasBufferCreateInfo(*DebugNameString);
	AccelerationStructureBuffer = ResourceCast(RHICreateBuffer(SizeInfo.ResultSize, BUF_AccelerationStructure, 0, ERHIAccess::BVHWrite, BlasBufferCreateInfo).GetReference());

	VkDevice NativeDevice = Device->GetInstanceHandle();

	VkAccelerationStructureCreateInfoKHR CreateInfo;
	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
	CreateInfo.buffer = AccelerationStructureBuffer->GetHandle();
	CreateInfo.offset = AccelerationStructureBuffer->GetOffset();
	CreateInfo.size = SizeInfo.ResultSize;
	CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateAccelerationStructureKHR(NativeDevice, &CreateInfo, VULKAN_CPU_ALLOCATOR, &Handle));
	
	VkAccelerationStructureDeviceAddressInfoKHR DeviceAddressInfo;
	ZeroVulkanStruct(DeviceAddressInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
	DeviceAddressInfo.accelerationStructure = Handle;
	Address = VulkanDynamicAPI::vkGetAccelerationStructureDeviceAddressKHR(NativeDevice, &DeviceAddressInfo);
}

FVulkanRayTracingGeometry::~FVulkanRayTracingGeometry()
{
	if (Handle != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::AccelerationStructure, Handle);
	}
	RemoveCompactionRequest();
}

void FVulkanRayTracingGeometry::SetInitializer(const FRayTracingGeometryInitializer& InInitializer)
{
	checkf(InitializedType == ERayTracingGeometryInitializerType::StreamingDestination, TEXT("Only FVulkanRayTracingGeometry that was created as StreamingDestination can update their initializer."));
	Initializer = InInitializer;

	// TODO: Update HitGroup Parameters
}

void FVulkanRayTracingGeometry::Swap(FVulkanRayTracingGeometry& Other)
{
	::Swap(Handle, Other.Handle);
	::Swap(Address, Other.Address);
	::Swap(AccelerationStructureCompactedSize, Other.AccelerationStructureCompactedSize);

	AccelerationStructureBuffer = Other.AccelerationStructureBuffer;

	// The rest of the members should be updated using SetInitializer()
}

void FVulkanRayTracingGeometry::RemoveCompactionRequest()
{
	if (bHasPendingCompactionRequests)
	{
		check(AccelerationStructureBuffer);
		bool bRequestFound = Device->GetRayTracingCompactionRequestHandler()->ReleaseRequest(this);
		check(bRequestFound);
		bHasPendingCompactionRequests = false;
	}
}

void FVulkanRayTracingGeometry::CompactAccelerationStructure(FVulkanCmdBuffer& CmdBuffer, uint64 InSizeAfterCompaction)
{
	check(bHasPendingCompactionRequests);
	bHasPendingCompactionRequests = false;

	ensureMsgf(InSizeAfterCompaction > 0, TEXT("Compacted acceleration structure size is expected to be non-zero. This error suggests that GPU readback synchronization is broken."));
	if (InSizeAfterCompaction == 0)
	{
		return;
	}

	// Move old AS into this temporary variable which gets released when this function returns	
	TRefCountPtr<FVulkanResourceMultiBuffer> OldAccelerationStructure = AccelerationStructureBuffer;
	VkAccelerationStructureKHR OldHandle = Handle;

	FString DebugNameString = Initializer.DebugName.ToString();
	FRHIResourceCreateInfo BlasBufferCreateInfo(*DebugNameString);
	AccelerationStructureBuffer = new FVulkanResourceMultiBuffer(Device, InSizeAfterCompaction, BUF_AccelerationStructure, 0, BlasBufferCreateInfo);
	
	VkDevice NativeDevice = Device->GetInstanceHandle();

	VkAccelerationStructureCreateInfoKHR CreateInfo;
	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
	CreateInfo.buffer = AccelerationStructureBuffer->GetHandle();
	CreateInfo.offset = AccelerationStructureBuffer->GetOffset();
	CreateInfo.size = InSizeAfterCompaction;
	CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateAccelerationStructureKHR(NativeDevice, &CreateInfo, VULKAN_CPU_ALLOCATOR, &Handle));

	VkAccelerationStructureDeviceAddressInfoKHR DeviceAddressInfo;
	ZeroVulkanStruct(DeviceAddressInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
	DeviceAddressInfo.accelerationStructure = Handle;
	Address = VulkanDynamicAPI::vkGetAccelerationStructureDeviceAddressKHR(NativeDevice, &DeviceAddressInfo);

	// Add a barrier to make sure acceleration structure are synchronized correctly for the copy command.
	AddAccelerationStructureBuildBarrier(CmdBuffer.GetHandle());

	VkCopyAccelerationStructureInfoKHR CopyInfo;
	ZeroVulkanStruct(CopyInfo, VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR);
	CopyInfo.src = OldHandle;
	CopyInfo.dst = Handle;
	CopyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
	VulkanDynamicAPI::vkCmdCopyAccelerationStructureKHR(CmdBuffer.GetHandle(), &CopyInfo);

	AccelerationStructureCompactedSize = InSizeAfterCompaction;

	Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::AccelerationStructure, OldHandle);
}

static void GetTLASBuildData(
	const VkDevice Device,
	const uint32 NumInstances,
	const VkDeviceAddress InstanceBufferAddress,
	FVkRtTLASBuildData& BuildData)
{
	VkDeviceOrHostAddressConstKHR InstanceBufferDeviceAddress = {};
	InstanceBufferDeviceAddress.deviceAddress = InstanceBufferAddress;

	BuildData.Geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	BuildData.Geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	BuildData.Geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	BuildData.Geometry.geometry.instances.data = InstanceBufferDeviceAddress;

	BuildData.GeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	BuildData.GeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	BuildData.GeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	BuildData.GeometryInfo.geometryCount = 1;
	BuildData.GeometryInfo.pGeometries = &BuildData.Geometry;

	VulkanDynamicAPI::vkGetAccelerationStructureBuildSizesKHR(
		Device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&BuildData.GeometryInfo,
		&NumInstances,
		&BuildData.SizesInfo);
}

static VkGeometryInstanceFlagsKHR TranslateRayTracingInstanceFlags(ERayTracingInstanceFlags InFlags)
{
	VkGeometryInstanceFlagsKHR Result = 0;

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::TriangleCullDisable))
	{
		Result |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	}

	if (!EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::TriangleCullReverse))
	{
		// Counterclockwise is the default for UE.
		Result |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
	}

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::ForceOpaque))
	{
		Result |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	}

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::ForceNonOpaque))
	{
		Result |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;
	}

	return Result;
}

// This structure is analogous to FHitGroupSystemParameters in D3D12 RHI.
// However, it only contains generic parameters that do not require a full shader binding table (i.e. no per-hit-group user data).
// It is designed to be used to access vertex and index buffers during inline ray tracing.
struct FVulkanRayTracingGeometryParameters
{
	union
	{
		struct
		{
			uint32 IndexStride : 8; // Can be just 1 bit to indicate 16 or 32 bit indices
			uint32 VertexStride : 8; // Can be just 2 bits to indicate float3, float2 or half2 format
			uint32 Unused : 16;
		} Config;
		uint32 ConfigBits = 0;
	};
	uint32 IndexBufferOffsetInBytes = 0;
	uint64 IndexBuffer = 0;
	uint64 VertexBuffer = 0;
};

FVulkanRayTracingScene::FVulkanRayTracingScene(FRayTracingSceneInitializer2 InInitializer, FVulkanDevice* InDevice)
	: Device(InDevice), Initializer(MoveTemp(InInitializer))
{
	const ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace; // #yuriy_todo: pass this in

	SizeInfo = {};

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();
	check(NumLayers > 0);

	Layers.SetNum(NumLayers);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayerData& Layer = Layers[LayerIndex];

		FRayTracingAccelerationStructureSize LayerSizeInfo = RHICalcRayTracingSceneSize(Initializer.NumNativeInstancesPerLayer[LayerIndex], BuildFlags);

		Layer.BufferOffset = Align(SizeInfo.ResultSize, GRHIRayTracingAccelerationStructureAlignment);
		Layer.ScratchBufferOffset = Align(SizeInfo.BuildScratchSize, GRHIRayTracingScratchBufferAlignment);

		SizeInfo.ResultSize = Layer.BufferOffset + LayerSizeInfo.ResultSize;
		SizeInfo.BuildScratchSize = Layer.BufferOffset + LayerSizeInfo.BuildScratchSize;
	}

	const uint32 ParameterBufferSize = FMath::Max<uint32>(1, Initializer.NumTotalSegments) * sizeof(FVulkanRayTracingGeometryParameters);
	FRHIResourceCreateInfo ParameterBufferCreateInfo(TEXT("RayTracingSceneMetadata"));
	PerInstanceGeometryParameterBuffer = new FVulkanResourceMultiBuffer(Device,
		ParameterBufferSize, BUF_StructuredBuffer | BUF_ShaderResource, sizeof(FVulkanRayTracingGeometryParameters),
		ParameterBufferCreateInfo);

	PerInstanceGeometryParameterSRV = new FVulkanShaderResourceView(Device, PerInstanceGeometryParameterBuffer, 0);
}

FVulkanRayTracingScene::~FVulkanRayTracingScene()
{
}

void FVulkanRayTracingScene::BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset)
{
	check(IsInRHIThread() || !IsRunningRHIInSeparateThread());

	check(SizeInfo.ResultSize + InBufferOffset <= InBuffer->GetSize());
	
	AccelerationStructureBuffer = ResourceCast(InBuffer);

	for (auto& Layer : Layers)
	{
		checkf(Layer.ShaderResourceView == nullptr, TEXT("Binding multiple buffers is not currently supported."));

		const uint32 LayerOffset = InBufferOffset + Layer.BufferOffset;
		check(LayerOffset % GRHIRayTracingAccelerationStructureAlignment == 0);

		Layer.ShaderResourceView = new FVulkanShaderResourceView(Device, AccelerationStructureBuffer, LayerOffset);
	}
}

void FVulkanRayTracingScene::BuildAccelerationStructure(
	FVulkanCommandListContext& CommandContext,
	FVulkanResourceMultiBuffer* InScratchBuffer, uint32 InScratchOffset,
	FVulkanResourceMultiBuffer* InInstanceBuffer, uint32 InInstanceOffset)
{
	// Build a metadata buffer	that contains VulkanRHI-specific per-geometry parameters that allow us to access
	// vertex and index buffers from shaders that use inline ray tracing.
	BuildPerInstanceGeometryParameterBuffer(CommandContext);

	check(AccelerationStructureBuffer.IsValid());
	check(InInstanceBuffer != nullptr);

	TRefCountPtr<FVulkanResourceMultiBuffer> ScratchBuffer;

	if (InScratchBuffer == nullptr)
	{
		FRHIResourceCreateInfo ScratchBufferCreateInfo(TEXT("BuildScratchTLAS"));
		ScratchBuffer = ResourceCast(RHICreateBuffer(SizeInfo.BuildScratchSize, BUF_StructuredBuffer | BUF_RayTracingScratch, 0, ERHIAccess::UAVCompute, ScratchBufferCreateInfo).GetReference());
		InScratchBuffer = ScratchBuffer.GetReference();
		InScratchOffset = 0;
	}

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();

	TArray<FVkRtTLASBuildData> BuildDatas;
	BuildDatas.SetNum(NumLayers);

	TArray<VkAccelerationStructureBuildGeometryInfoKHR> GeometryInfos;
	GeometryInfos.SetNum(NumLayers);

	TArray<VkAccelerationStructureBuildRangeInfoKHR> BuildRanges;
	BuildRanges.SetNum(NumLayers);

	TArray<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRanges;
	pBuildRanges.SetNum(NumLayers);

	uint32 InstanceBaseOffset = 0;

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FLayerData& Layer = Layers[LayerIndex];

		VkDeviceAddress InstanceBufferAddress = InInstanceBuffer->GetDeviceAddress() + InInstanceOffset;

		FVkRtTLASBuildData& BuildData = BuildDatas[LayerIndex];
		GetTLASBuildData(Device->GetInstanceHandle(), Initializer.NumNativeInstancesPerLayer[LayerIndex], InstanceBufferAddress, BuildData);

		checkf(Layer.ShaderResourceView, TEXT("A buffer must be bound to the ray tracing scene before it can be built."));
		BuildData.GeometryInfo.dstAccelerationStructure = Layer.ShaderResourceView->AccelerationStructureHandle;
		BuildData.GeometryInfo.scratchData.deviceAddress = InScratchBuffer->GetDeviceAddress() + InScratchOffset + Layer.ScratchBufferOffset;

		GeometryInfos[LayerIndex] = BuildData.GeometryInfo;

		VkAccelerationStructureBuildRangeInfoKHR& TLASBuildRangeInfo = BuildRanges[LayerIndex];
		TLASBuildRangeInfo.primitiveCount = Initializer.NumNativeInstancesPerLayer[LayerIndex];
		TLASBuildRangeInfo.primitiveOffset = InstanceBaseOffset;
		TLASBuildRangeInfo.transformOffset = 0;
		TLASBuildRangeInfo.firstVertex = 0;

		pBuildRanges[LayerIndex] = &TLASBuildRangeInfo;

		InstanceBaseOffset += Initializer.NumNativeInstancesPerLayer[LayerIndex];
	}

	FVulkanCommandBufferManager& CommandBufferManager = *CommandContext.GetCommandBufferManager();
	FVulkanCmdBuffer* const CmdBuffer = CommandBufferManager.GetActiveCmdBuffer();

	// Force a memory barrier to make sure all previous builds ops are finished before building the TLAS
	AddAccelerationStructureBuildBarrier(CmdBuffer->GetHandle());

	VulkanDynamicAPI::vkCmdBuildAccelerationStructuresKHR(CmdBuffer->GetHandle(), NumLayers, GeometryInfos.GetData(), pBuildRanges.GetData());

	// Acceleration structure build barrier is used here to ensure that the acceleration structure build is complete before any rays are traced
	AddAccelerationStructureBuildBarrier(CmdBuffer->GetHandle());

	CommandBufferManager.SubmitActiveCmdBuffer();
	CommandBufferManager.PrepareForNewActiveCommandBuffer();
}

void FVulkanRayTracingScene::BuildPerInstanceGeometryParameterBuffer(FVulkanCommandListContext& CommandContext)
{
	// TODO: we could cache parameters in the geometry object to avoid some of the pointer chasing (if this is measured to be a performance issue)

	const uint32 ParameterBufferSize = FMath::Max<uint32>(1, Initializer.NumTotalSegments) * sizeof(FVulkanRayTracingGeometryParameters);
	check(PerInstanceGeometryParameterBuffer->GetSize() >= ParameterBufferSize);

	check(IsInRHIThread() || !IsRunningRHIInSeparateThread());

	void* MappedBuffer = PerInstanceGeometryParameterBuffer->Lock(CommandContext, RLM_WriteOnly, ParameterBufferSize, 0);
	FVulkanRayTracingGeometryParameters* MappedParameters = reinterpret_cast<FVulkanRayTracingGeometryParameters*>(MappedBuffer);
	uint32 ParameterIndex = 0;

	for (FRHIRayTracingGeometry* GeometryRHI : Initializer.PerInstanceGeometries)
	{
		const FVulkanRayTracingGeometry* Geometry = ResourceCast(GeometryRHI);
		const FRayTracingGeometryInitializer& GeometryInitializer = Geometry->GetInitializer();

		const FVulkanResourceMultiBuffer* IndexBuffer = ResourceCast(GeometryInitializer.IndexBuffer.GetReference());

		const uint32 IndexStride = IndexBuffer ? IndexBuffer->GetStride() : 0;
		const uint32 IndexOffsetInBytes = GeometryInitializer.IndexBufferOffset;
		const VkDeviceAddress IndexBufferAddress = IndexBuffer ? IndexBuffer->GetDeviceAddress() : VkDeviceAddress(0);

		for (const FRayTracingGeometrySegment& Segment : GeometryInitializer.Segments)
		{
			const FVulkanResourceMultiBuffer* VertexBuffer = ResourceCast(Segment.VertexBuffer.GetReference());
			checkf(VertexBuffer, TEXT("All ray tracing geometry segments must have a valid vertex buffer"));
			const VkDeviceAddress VertexBufferAddress = VertexBuffer->GetDeviceAddress();

			FVulkanRayTracingGeometryParameters SegmentParameters;
			SegmentParameters.Config.IndexStride = IndexStride;
			SegmentParameters.Config.VertexStride = Segment.VertexBufferStride;

			if (IndexStride)
			{
				SegmentParameters.IndexBufferOffsetInBytes = IndexOffsetInBytes + IndexStride * Segment.FirstPrimitive * 3;
				SegmentParameters.IndexBuffer = static_cast<uint64>(IndexBufferAddress);
			}
			else
			{
				SegmentParameters.IndexBuffer = 0;
			}

			SegmentParameters.VertexBuffer = static_cast<uint64>(VertexBufferAddress) + Segment.VertexBufferOffset;

			check(ParameterIndex < Initializer.NumTotalSegments);
			MappedParameters[ParameterIndex] = SegmentParameters;
			ParameterIndex++;
		}
	}

	check(ParameterIndex == Initializer.NumTotalSegments);

	PerInstanceGeometryParameterBuffer->Unlock(CommandContext);
}

void FVulkanDynamicRHI::RHITransferRayTracingGeometryUnderlyingResource(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry)
{
	check(DestGeometry);
	FVulkanRayTracingGeometry* Dest = ResourceCast(DestGeometry);
	if (!SrcGeometry)
	{
		TRefCountPtr<FVulkanRayTracingGeometry> DeletionProxy = new FVulkanRayTracingGeometry(NoInit);
		Dest->RemoveCompactionRequest();
		Dest->Swap(*DeletionProxy);
	}
	else
	{
		FVulkanRayTracingGeometry* Src = ResourceCast(SrcGeometry);
		Dest->Swap(*Src);
	}
}

FRayTracingAccelerationStructureSize FVulkanDynamicRHI::RHICalcRayTracingSceneSize(uint32 MaxInstances, ERayTracingAccelerationStructureFlags Flags)
{
	FVkRtTLASBuildData BuildData;
	const VkDeviceAddress InstanceBufferAddress = 0; // No device address available when only querying TLAS size
	GetTLASBuildData(Device->GetInstanceHandle(), MaxInstances, InstanceBufferAddress, BuildData);

	FRayTracingAccelerationStructureSize Result;
	Result.ResultSize = BuildData.SizesInfo.accelerationStructureSize;
	Result.BuildScratchSize = BuildData.SizesInfo.buildScratchSize;
	Result.UpdateScratchSize = BuildData.SizesInfo.updateScratchSize;

	return Result;
}

FRayTracingAccelerationStructureSize FVulkanDynamicRHI::RHICalcRayTracingGeometrySize(const FRayTracingGeometryInitializer& Initializer)
{	
	const uint32 IndexStrideInBytes = Initializer.IndexBuffer ? Initializer.IndexBuffer->GetStride() : 0;

	FVkRtBLASBuildData BuildData;
	GetBLASBuildData(
		Device->GetInstanceHandle(),
		MakeArrayView(Initializer.Segments),
		Initializer.GeometryType,
		Initializer.IndexBuffer,
		Initializer.IndexBufferOffset,
		IndexStrideInBytes,
		GetRayTracingAccelerationStructureBuildFlags(Initializer),
		EAccelerationStructureBuildMode::Build,
		BuildData);

	FRayTracingAccelerationStructureSize Result;
	Result.ResultSize = Align(BuildData.SizesInfo.accelerationStructureSize, GRHIRayTracingAccelerationStructureAlignment);
	Result.BuildScratchSize = Align(BuildData.SizesInfo.buildScratchSize, GRHIRayTracingScratchBufferAlignment);
	Result.UpdateScratchSize = Align(BuildData.SizesInfo.updateScratchSize, GRHIRayTracingScratchBufferAlignment);
	
	return Result;
}

FRayTracingSceneRHIRef FVulkanDynamicRHI::RHICreateRayTracingScene(FRayTracingSceneInitializer2 Initializer)
{
	return new FVulkanRayTracingScene(MoveTemp(Initializer), GetDevice());
}

FRayTracingGeometryRHIRef FVulkanDynamicRHI::RHICreateRayTracingGeometry(const FRayTracingGeometryInitializer& Initializer)
{
	return new FVulkanRayTracingGeometry(Initializer, GetDevice());
}

void FVulkanCommandListContext::RHIClearRayTracingBindings(FRHIRayTracingScene* Scene)
{
	 // TODO
}

void FVulkanCommandListContext::RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset)
{
	ResourceCast(Scene)->BindBuffer(Buffer, BufferOffset);
}

// Todo: High level rhi call should have transitioned and verified vb and ib to read for each segment
void FVulkanCommandListContext::RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange)
{
	checkf(ScratchBufferRange.Buffer != nullptr, TEXT("BuildAccelerationStructures requires valid scratch buffer"));

	// Update geometry vertex buffers
	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FVulkanRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());

		if (P.Segments.Num())
		{
			checkf(P.Segments.Num() == Geometry->Initializer.Segments.Num(),
				TEXT("If updated segments are provided, they must exactly match existing geometry segments. Only vertex buffer bindings may change."));

			for (int32 i = 0; i < P.Segments.Num(); ++i)
			{
				checkf(P.Segments[i].MaxVertices <= Geometry->Initializer.Segments[i].MaxVertices,
					TEXT("Maximum number of vertices in a segment (%u) must not be smaller than what was declared during FRHIRayTracingGeometry creation (%u), as this controls BLAS memory allocation."),
					P.Segments[i].MaxVertices, Geometry->Initializer.Segments[i].MaxVertices
				);

				Geometry->Initializer.Segments[i].VertexBuffer = P.Segments[i].VertexBuffer;
				Geometry->Initializer.Segments[i].VertexBufferElementType = P.Segments[i].VertexBufferElementType;
				Geometry->Initializer.Segments[i].VertexBufferStride = P.Segments[i].VertexBufferStride;
				Geometry->Initializer.Segments[i].VertexBufferOffset = P.Segments[i].VertexBufferOffset;
			}
		}
	}

	uint32 ScratchBufferSize = ScratchBufferRange.Size ? ScratchBufferRange.Size : ScratchBufferRange.Buffer->GetSize();

	checkf(ScratchBufferSize + ScratchBufferRange.Offset <= ScratchBufferRange.Buffer->GetSize(),
		TEXT("BLAS scratch buffer range size is %lld bytes with offset %lld, but the buffer only has %lld bytes. "),
		ScratchBufferRange.Size, ScratchBufferRange.Offset, ScratchBufferRange.Buffer->GetSize());

	const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
	FVulkanResourceMultiBuffer* ScratchBuffer = ResourceCast(ScratchBufferRange.Buffer);
	uint32 ScratchBufferOffset = ScratchBufferRange.Offset;

	TArray<FVkRtBLASBuildData, TInlineAllocator<32>> TempBuildData;
	TArray<VkAccelerationStructureBuildGeometryInfoKHR, TInlineAllocator<32>> BuildGeometryInfos;
	TArray<VkAccelerationStructureBuildRangeInfoKHR*, TInlineAllocator<32>> BuildRangeInfos;
	TempBuildData.Reserve(Params.Num());
	BuildGeometryInfos.Reserve(Params.Num());
	BuildRangeInfos.Reserve(Params.Num());	

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FVulkanRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());
		const bool bIsUpdate = P.BuildMode == EAccelerationStructureBuildMode::Update;

		uint64 ScratchBufferRequiredSize = bIsUpdate ? Geometry->SizeInfo.UpdateScratchSize : Geometry->SizeInfo.BuildScratchSize;
		checkf(ScratchBufferRequiredSize + ScratchBufferOffset <= ScratchBufferSize,
			TEXT("BLAS scratch buffer size is %ld bytes with offset %ld (%ld bytes available), but the build requires %lld bytes. "),
			ScratchBufferSize, ScratchBufferOffset, ScratchBufferSize - ScratchBufferOffset, ScratchBufferRequiredSize);

		FVkRtBLASBuildData& BuildData = TempBuildData.AddDefaulted_GetRef();
		GetBLASBuildData(
			Device->GetInstanceHandle(),
			MakeArrayView(Geometry->Initializer.Segments),
			Geometry->Initializer.GeometryType,
			Geometry->Initializer.IndexBuffer,
			Geometry->Initializer.IndexBufferOffset,			
			Geometry->Initializer.IndexBuffer ? Geometry->Initializer.IndexBuffer->GetStride() : 0,
			GetRayTracingAccelerationStructureBuildFlags(Geometry->Initializer),
			P.BuildMode,
			BuildData);

		check(BuildData.SizesInfo.accelerationStructureSize <= Geometry->AccelerationStructureBuffer->GetSize());

		BuildData.GeometryInfo.dstAccelerationStructure = Geometry->Handle;
		BuildData.GeometryInfo.srcAccelerationStructure = bIsUpdate ? Geometry->Handle : VK_NULL_HANDLE;

		VkDeviceAddress ScratchBufferAddress = ScratchBuffer->GetDeviceAddress() + ScratchBufferOffset;
		ScratchBufferOffset += ScratchBufferRequiredSize;

		checkf(ScratchBufferAddress % GRHIRayTracingScratchBufferAlignment == 0,
			TEXT("BLAS scratch buffer (plus offset) must be aligned to %ld bytes."),
			GRHIRayTracingScratchBufferAlignment);

		BuildData.GeometryInfo.scratchData.deviceAddress = ScratchBufferAddress;

		VkAccelerationStructureBuildRangeInfoKHR* const pBuildRanges = BuildData.Ranges.GetData();

		BuildGeometryInfos.Add(BuildData.GeometryInfo);
		BuildRangeInfos.Add(pBuildRanges);
	}
	
	FVulkanCmdBuffer* const CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VulkanDynamicAPI::vkCmdBuildAccelerationStructuresKHR(CmdBuffer->GetHandle(), Params.Num(), BuildGeometryInfos.GetData(), BuildRangeInfos.GetData());

	// Add an acceleration structure build barrier after each acceleration structure build batch.
	// This is required because there are currently no explicit read/write barriers
	// for acceleration structures, but we need to ensure that all commands
	// are complete before BLAS is used again on the GPU.
	AddAccelerationStructureBuildBarrier(CmdBuffer->GetHandle());

	CommandBufferManager->SubmitActiveCmdBuffer();
	CommandBufferManager->PrepareForNewActiveCommandBuffer();

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FVulkanRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());

		ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(Geometry->Initializer);
		if (ShouldCompactAfterBuild(GeometryBuildFlags))
		{
			Device->GetRayTracingCompactionRequestHandler()->RequestCompact(Geometry);
			Geometry->bHasPendingCompactionRequests = true;
		}
	}
}

void FVulkanCommandListContext::RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams)
{
	FVulkanRayTracingScene* const Scene = ResourceCast(SceneBuildParams.Scene);
	FVulkanResourceMultiBuffer* const ScratchBuffer = ResourceCast(SceneBuildParams.ScratchBuffer);
	FVulkanResourceMultiBuffer* const InstanceBuffer = ResourceCast(SceneBuildParams.InstanceBuffer);
	Scene->BuildAccelerationStructure(
		*this, 
		ScratchBuffer, SceneBuildParams.ScratchBufferOffset, 
		InstanceBuffer, SceneBuildParams.InstanceBufferOffset);
}

void FVulkanCommandListContext::RHIRayTraceOcclusion(FRHIRayTracingScene* Scene, FRHIShaderResourceView* Rays, FRHIUnorderedAccessView* Output, uint32 NumRays)
{
	// todo
	return;
}

template<typename ShaderType>
static FRHIRayTracingShader* GetBuiltInRayTracingShader()
{
	const FGlobalShaderMap* const ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	auto Shader = ShaderMap->GetShader<ShaderType>();
	return static_cast<FRHIRayTracingShader*>(Shader.GetRayTracingShader());
}

void FVulkanDevice::InitializeRayTracing()
{
	check(BasicRayTracingPipeline == nullptr);
	// the pipeline should be initialized on the first use due to the ability to disable RT in the game settings
	//BasicRayTracingPipeline = new FVulkanBasicRaytracingPipeline(this);
}

void FVulkanDevice::CleanUpRayTracing()
{
	if (BasicRayTracingPipeline != nullptr)
	{
		delete BasicRayTracingPipeline;
		BasicRayTracingPipeline = nullptr;
	}
}

static uint32 GetAlignedSize(uint32 Value, uint32 Alignment)
{
	return (Value + Alignment - 1) & ~(Alignment - 1);
}

FVulkanRayTracingPipelineState::FVulkanRayTracingPipelineState(FVulkanDevice* const InDevice, const FRayTracingPipelineStateInitializer& Initializer)
{	
	check(Layout == nullptr);

	TArrayView<FRHIRayTracingShader*> InitializerRayGenShaders = Initializer.GetRayGenTable();
	TArrayView<FRHIRayTracingShader*> InitializerMissShaders = Initializer.GetMissTable();
	TArrayView<FRHIRayTracingShader*> InitializerHitGroupShaders = Initializer.GetHitGroupTable();
	// vkrt todo: Callable shader support

	FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
	FUniformBufferGatherInfo UBGatherInfo;
	
	for (FRHIRayTracingShader* RayGenShader : InitializerRayGenShaders)
	{
		const FVulkanShaderHeader& Header = static_cast<FVulkanRayGenShader*>(RayGenShader)->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, ShaderStage::RayGen, Header, UBGatherInfo);
	}

	for (FRHIRayTracingShader* MissShader : InitializerMissShaders)
	{
		const FVulkanShaderHeader& Header = static_cast<FVulkanRayMissShader*>(MissShader)->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_MISS_BIT_KHR, ShaderStage::RayMiss, Header, UBGatherInfo);
	}

	for (FRHIRayTracingShader* HitGroupShader : InitializerHitGroupShaders)
	{
		const FVulkanShaderHeader& Header = static_cast<FVulkanRayHitGroupShader*>(HitGroupShader)->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ShaderStage::RayHitGroup, Header, UBGatherInfo);
		// vkrt todo: How to handle any hit for hit group?
	}

	DescriptorSetLayoutInfo.FinalizeBindings<false>(*InDevice, UBGatherInfo, TArrayView<FRHISamplerState*>());

	Layout = new FVulkanRayTracingLayout(InDevice);
	Layout->DescriptorSetLayout.CopyFrom(DescriptorSetLayoutInfo);
	FVulkanDescriptorSetLayoutMap DSetLayoutMap;
	Layout->Compile(DSetLayoutMap);

	TArray<VkPipelineShaderStageCreateInfo> ShaderStages;
	TArray<VkRayTracingShaderGroupCreateInfoKHR> ShaderGroups;
	TArray<ANSICHAR*> EntryPointNames;
	const uint32 EntryPointNameMaxLength = 24;

	for (FRHIRayTracingShader* const RayGenShaderRHI : InitializerRayGenShaders)
	{
		VkPipelineShaderStageCreateInfo ShaderStage;
		ZeroVulkanStruct(ShaderStage, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
		ShaderStage.module = static_cast<FVulkanRayGenShader*>(RayGenShaderRHI)->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash())->GetVkShaderModule();
		ShaderStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			
		ANSICHAR* const EntryPoint = new ANSICHAR[EntryPointNameMaxLength];
		static_cast<FVulkanRayGenShader*>(RayGenShaderRHI)->GetEntryPoint(EntryPoint, EntryPointNameMaxLength);
		EntryPointNames.Add(EntryPoint);
		ShaderStage.pName = EntryPoint;
		ShaderStages.Add(ShaderStage);

		VkRayTracingShaderGroupCreateInfoKHR ShaderGroup;
		ZeroVulkanStruct(ShaderGroup, VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
		ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		ShaderGroup.generalShader = ShaderStages.Num() - 1;
		ShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		ShaderGroups.Add(ShaderGroup);
	}

	for (FRHIRayTracingShader* const MissShaderRHI : InitializerMissShaders)
	{
		VkPipelineShaderStageCreateInfo ShaderStage;
		ZeroVulkanStruct(ShaderStage, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
		ShaderStage.module = static_cast<FVulkanRayMissShader*>(MissShaderRHI)->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash())->GetVkShaderModule();
		ShaderStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;

		ANSICHAR* const EntryPoint = new char[EntryPointNameMaxLength];
		static_cast<FVulkanRayGenShader*>(MissShaderRHI)->GetEntryPoint(EntryPoint, EntryPointNameMaxLength);
		EntryPointNames.Add(EntryPoint);
		ShaderStage.pName = EntryPoint;
		ShaderStages.Add(ShaderStage);

		VkRayTracingShaderGroupCreateInfoKHR ShaderGroup;
		ZeroVulkanStruct(ShaderGroup, VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
		ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		ShaderGroup.generalShader = ShaderStages.Num() - 1;
		ShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		ShaderGroups.Add(ShaderGroup);
	}

	for (FRHIRayTracingShader* const HitGroupShaderRHI : InitializerHitGroupShaders)
	{
		VkPipelineShaderStageCreateInfo ShaderStage;
		ZeroVulkanStruct(ShaderStage, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
		ShaderStage.module = static_cast<FVulkanRayHitGroupShader*>(HitGroupShaderRHI)->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash())->GetVkShaderModule();
		ShaderStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		ANSICHAR* const EntryPoint = new char[EntryPointNameMaxLength];
		static_cast<FVulkanRayHitGroupShader*>(HitGroupShaderRHI)->GetEntryPoint(EntryPoint, EntryPointNameMaxLength);
		EntryPointNames.Add(EntryPoint);
		ShaderStage.pName = EntryPoint;
		ShaderStages.Add(ShaderStage);

		VkRayTracingShaderGroupCreateInfoKHR ShaderGroup;
		ZeroVulkanStruct(ShaderGroup, VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
		ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		ShaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
		ShaderGroup.closestHitShader = ShaderStages.Num() - 1;
		ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR; // vkrt: todo
		ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		ShaderGroups.Add(ShaderGroup);
	}

	VkRayTracingPipelineCreateInfoKHR RayTracingPipelineCreateInfo;
	ZeroVulkanStruct(RayTracingPipelineCreateInfo, VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);
	RayTracingPipelineCreateInfo.stageCount = ShaderStages.Num();
	RayTracingPipelineCreateInfo.pStages = ShaderStages.GetData();
	RayTracingPipelineCreateInfo.groupCount = ShaderGroups.Num();
	RayTracingPipelineCreateInfo.pGroups = ShaderGroups.GetData();
	RayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
	RayTracingPipelineCreateInfo.layout = Layout->GetPipelineLayout();
	
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateRayTracingPipelinesKHR(
		InDevice->GetInstanceHandle(), 
		VK_NULL_HANDLE, // Deferred Operation 
		VK_NULL_HANDLE, // Pipeline Cache 
		1, 
		&RayTracingPipelineCreateInfo, 
		VULKAN_CPU_ALLOCATOR, 
		&Pipeline));

	for (ANSICHAR* const EntryPoint : EntryPointNames)
	{
		delete[] EntryPoint;
	}

	const FRayTracingProperties& Props = InDevice->GetRayTracingProperties();
	const uint32 HandleSize = Props.RayTracingPipeline.shaderGroupHandleSize;
	const uint32 HandleSizeAligned = GetAlignedSize(HandleSize, Props.RayTracingPipeline.shaderGroupHandleAlignment);
	const uint32 GroupCount = ShaderGroups.Num();
	const uint32 SBTSize = GroupCount * HandleSizeAligned;

	TArray<uint8> ShaderHandleStorage;
	ShaderHandleStorage.AddUninitialized(SBTSize);
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkGetRayTracingShaderGroupHandlesKHR(InDevice->GetInstanceHandle(), Pipeline, 0, GroupCount, SBTSize, ShaderHandleStorage.GetData()));

	auto CopyHandlesToSBT = [InDevice, HandleSize, ShaderHandleStorage](FVkRtAllocation& Allocation, uint32 Offset)
	{
		FVulkanRayTracingAllocator::Allocate(
			InDevice,
			HandleSize,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Allocation);

		void* pMappedBufferMemory = nullptr;
		VERIFYVULKANRESULT(VulkanRHI::vkMapMemory(InDevice->GetInstanceHandle(), Allocation.Memory, 0, VK_WHOLE_SIZE, 0, &pMappedBufferMemory));
		{
			FMemory::Memcpy(pMappedBufferMemory, ShaderHandleStorage.GetData() + Offset, HandleSize);
		}
		VulkanRHI::vkUnmapMemory(InDevice->GetInstanceHandle(), Allocation.Memory);
	};

	CopyHandlesToSBT(RayGenShaderBindingTable, 0);
	CopyHandlesToSBT(MissShaderBindingTable, HandleSizeAligned);
	CopyHandlesToSBT(HitShaderBindingTable, HandleSizeAligned * 2);
}

FVulkanRayTracingPipelineState::~FVulkanRayTracingPipelineState()
{
	FVulkanRayTracingAllocator::Free(RayGenShaderBindingTable);
	FVulkanRayTracingAllocator::Free(MissShaderBindingTable);
	FVulkanRayTracingAllocator::Free(HitShaderBindingTable);

	if (Layout != nullptr)
	{
		delete Layout;
		Layout = nullptr;
	}
}

FVulkanBasicRaytracingPipeline::FVulkanBasicRaytracingPipeline(FVulkanDevice* const InDevice)
{
	check(Occlusion == nullptr);

	// Occlusion pipeline
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS

		FRayTracingPipelineStateInitializer OcclusionInitializer;

		FRHIRayTracingShader* OcclusionRGSTable[] = { GetBuiltInRayTracingShader<FOcclusionMainRG>() };
		OcclusionInitializer.SetRayGenShaderTable(OcclusionRGSTable);

		FRHIRayTracingShader* OcclusionMSTable[] = { GetBuiltInRayTracingShader<FDefaultPayloadMS>() };
		OcclusionInitializer.SetMissShaderTable(OcclusionMSTable);

		FRHIRayTracingShader* OcclusionCHSTable[] = { GetBuiltInRayTracingShader<FDefaultMainCHS>() };
		OcclusionInitializer.SetHitGroupTable(OcclusionCHSTable);

		OcclusionInitializer.bAllowHitGroupIndexing = false;

		Occlusion = new FVulkanRayTracingPipelineState(InDevice, OcclusionInitializer);

		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

FVulkanBasicRaytracingPipeline::~FVulkanBasicRaytracingPipeline()
{
	if (Occlusion != nullptr)
	{
		delete Occlusion;
		Occlusion = nullptr;
	}
}

void FVulkanRayTracingCompactedSizeQueryPool::EndBatch(FVulkanCmdBuffer* InCmdBuffer)
{
	check(CmdBuffer == nullptr);
	CmdBuffer = InCmdBuffer;
	FenceSignaledCounter = InCmdBuffer->GetFenceSignaledCounter();
}

void FVulkanRayTracingCompactedSizeQueryPool::Reset(FVulkanCmdBuffer* InCmdBuffer)
{
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
	FenceSignaledCounter = 0;
	CmdBuffer = nullptr;
}

bool FVulkanRayTracingCompactedSizeQueryPool::TryGetResults(uint32 NumResults)
{
	if (CmdBuffer == nullptr) return false;

	const uint64 FenceCurrentSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
	if (FenceSignaledCounter > FenceCurrentSignaledCounter)
	{
		return false;
	}

	VkResult Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, 0, NumResults, NumResults * sizeof(uint64), QueryOutput.GetData(), sizeof(uint64), VK_QUERY_RESULT_WAIT_BIT);
	if (Result == VK_SUCCESS)
	{
		return true;
	}
	return false;
}

FVulkanRayTracingCompactionRequestHandler::FVulkanRayTracingCompactionRequestHandler(FVulkanDevice* const InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
{
	QueryPool = new FVulkanRayTracingCompactedSizeQueryPool(InDevice, GVulkanRayTracingMaxBatchedCompaction);
}

void FVulkanRayTracingCompactionRequestHandler::RequestCompact(FVulkanRayTracingGeometry* InRTGeometry)
{
	check(InRTGeometry->AccelerationStructureBuffer);
	ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(InRTGeometry->Initializer);
	check(EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction) &&
		EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::FastTrace) &&
		!EnumHasAnyFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate));

	FScopeLock Lock(&CS);
	PendingRequests.Add(InRTGeometry);
}

bool FVulkanRayTracingCompactionRequestHandler::ReleaseRequest(FVulkanRayTracingGeometry* InRTGeometry)
{
	FScopeLock Lock(&CS);

	// Remove from pending list, not found then try active requests
	if (PendingRequests.Remove(InRTGeometry) <= 0)
	{
		// If currently enqueued, then clear pointer to not handle the compaction request anymore			
		for (int32 BLASIndex = 0; BLASIndex < ActiveBLASes.Num(); ++BLASIndex)
		{
			if (ActiveRequests[BLASIndex] == InRTGeometry)
			{
				ActiveRequests[BLASIndex] = nullptr;
				return true;
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void FVulkanRayTracingCompactionRequestHandler::Update(FVulkanCommandListContext& InCommandContext)
{
	LLM_SCOPE_BYNAME(TEXT("FVulkanRT/Compaction"));
	FScopeLock Lock(&CS);

	if (ActiveBLASes.Num() > 0)
	{		
		FVulkanCommandBufferManager& CommandBufferManager = *InCommandContext.GetCommandBufferManager();
		FVulkanCmdBuffer* const CmdBuffer = CommandBufferManager.GetActiveCmdBuffer();

		if (QueryPool->TryGetResults(ActiveBLASes.Num()))
		{
			// Compact
			for (int32 BLASIndex = 0; BLASIndex < ActiveBLASes.Num(); ++BLASIndex)
			{
				if (ActiveRequests[BLASIndex] != nullptr)
				{
					ActiveRequests[BLASIndex]->CompactAccelerationStructure(*CmdBuffer, QueryPool->GetResultValue(BLASIndex));
				}
			}

			QueryPool->Reset(CmdBuffer);

			ActiveRequests.Empty(ActiveRequests.Num());
			ActiveBLASes.Empty(ActiveBLASes.Num());
		}
	}

	// build a new set of build requests to extract the build data	
	for (FVulkanRayTracingGeometry* RTGeometry : PendingRequests)
	{
		ActiveRequests.Add(RTGeometry);
		ActiveBLASes.Add(RTGeometry->Handle);

		// enqueued enough requests for this update round
		if (ActiveRequests.Num() >= GVulkanRayTracingMaxBatchedCompaction)
		{
			break;
		}
	}

	// Do we have requests?
	if (ActiveRequests.Num() > 0)
	{
		// clear out all of the pending requests, don't allow the array to shrink
		PendingRequests.RemoveAt(0, ActiveRequests.Num(), false);

		FVulkanCommandBufferManager& CommandBufferManager = *InCommandContext.GetCommandBufferManager();
		FVulkanCmdBuffer* const CmdBuffer = CommandBufferManager.GetActiveCmdBuffer();

		// Barrier here is not stricly necessary as it is added after the build.
		// AddAccelerationStructureBuildBarrier(CmdBuffer->GetHandle());

		// Write compacted size info from the selected requests
		VulkanDynamicAPI::vkCmdWriteAccelerationStructuresPropertiesKHR(
			CmdBuffer->GetHandle(),
			ActiveBLASes.Num(), ActiveBLASes.GetData(),
			VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
			QueryPool->GetHandle(),
			0
		);

		CommandBufferManager.SubmitActiveCmdBuffer();
		CommandBufferManager.PrepareForNewActiveCommandBuffer();

		QueryPool->EndBatch(CmdBuffer);
	}
}

#endif // VULKAN_RHI_RAYTRACING
