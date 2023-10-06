// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanTransientResourceAllocator.h"


FVulkanTransientHeap::FVulkanTransientHeap(const FInitializer& Initializer, FVulkanDevice* InDevice)
	: FRHITransientHeap(Initializer)
	, FDeviceChild(InDevice)
	, VulkanBuffer(VK_NULL_HANDLE)
{
	EBufferUsageFlags UEBufferUsageFlags = BUF_VertexBuffer | BUF_IndexBuffer | BUF_DrawIndirect 
		| BUF_UnorderedAccess | BUF_StructuredBuffer | BUF_ShaderResource | BUF_KeepCPUAccessible;

#if VULKAN_RHI_RAYTRACING
	if (InDevice->GetOptionalExtensions().HasRaytracingExtensions())
	{
		UEBufferUsageFlags |= BUF_RayTracingScratch;
		// AccelerationStructure not yet supported as TransientResource see FVulkanTransientResourceAllocator::CreateBuffer
		//UEBufferUsageFlags |= BUF_AccelerationStructure;
	}
#endif
	const bool bZeroSize = false;
	VkBufferUsageFlags BufferUsageFlags = FVulkanResourceMultiBuffer::UEToVKBufferUsageFlags(InDevice, UEBufferUsageFlags, bZeroSize);

	VkBufferCreateInfo BufferCreateInfo;
	ZeroVulkanStruct(BufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
	BufferCreateInfo.size = Initializer.Size;
	BufferCreateInfo.usage = BufferUsageFlags;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(InDevice->GetInstanceHandle(), &BufferCreateInfo, VULKAN_CPU_ALLOCATOR, &VulkanBuffer));

	// Find the alignment that works for everyone
	const uint32 MinBufferAlignment = FMath::Max<uint32>(Initializer.Alignment, FMemoryManager::CalculateBufferAlignment(*InDevice, UEBufferUsageFlags, bZeroSize));

	const EVulkanAllocationFlags AllocFlags = EVulkanAllocationFlags::Dedicated | EVulkanAllocationFlags::AutoBind;
	InDevice->GetMemoryManager().AllocateBufferMemory(InternalAllocation, VulkanBuffer, AllocFlags, TEXT("VulkanTransientHeap"), MinBufferAlignment);
}

FVulkanTransientHeap::~FVulkanTransientHeap()
{
	Device->GetMemoryManager().FreeVulkanAllocation(InternalAllocation);
	Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Buffer, VulkanBuffer);
	VulkanBuffer = VK_NULL_HANDLE;
}

VkDeviceMemory FVulkanTransientHeap::GetMemoryHandle()
{
	return InternalAllocation.GetDeviceMemoryHandle(Device);
}

FVulkanAllocation FVulkanTransientHeap::GetVulkanAllocation(const FRHITransientHeapAllocation& HeapAllocation)
{
	FVulkanTransientHeap* Heap = static_cast<FVulkanTransientHeap*>(HeapAllocation.Heap);
	check(Heap);

	FVulkanAllocation TransientAlloc;
	TransientAlloc.Reference(Heap->InternalAllocation);
	TransientAlloc.VulkanHandle = (uint64)Heap->VulkanBuffer;
	TransientAlloc.Offset += HeapAllocation.Offset;
	TransientAlloc.Size = HeapAllocation.Size;
	check((TransientAlloc.Offset + TransientAlloc.Size) <= Heap->InternalAllocation.Size);
	return TransientAlloc;
}

FVulkanTransientHeapCache* FVulkanTransientHeapCache::Create(FVulkanDevice* InDevice)
{
	FRHITransientHeapCache::FInitializer Initializer = FRHITransientHeapCache::FInitializer::CreateDefault();

	// Respect a minimum alignment
	Initializer.HeapAlignment = FMath::Max((uint32)InDevice->GetLimits().bufferImageGranularity, 256u);

	// Mix resource types onto the same heap.
	Initializer.bSupportsAllHeapFlags = true;

	return new FVulkanTransientHeapCache(Initializer, InDevice);
}

FVulkanTransientHeapCache::FVulkanTransientHeapCache(const FRHITransientHeapCache::FInitializer& Initializer, FVulkanDevice* InDevice)
	: FRHITransientHeapCache(Initializer)
	, FDeviceChild(InDevice)
{
}

FRHITransientHeap* FVulkanTransientHeapCache::CreateHeap(const FRHITransientHeap::FInitializer& HeapInitializer)
{
	return new FVulkanTransientHeap(HeapInitializer, Device);
}


FVulkanTransientResourceAllocator::FVulkanTransientResourceAllocator(FVulkanTransientHeapCache& InHeapCache)
	: FRHITransientResourceHeapAllocator(InHeapCache)
	, FDeviceChild(InHeapCache.GetParent())
{
}

FRHITransientTexture* FVulkanTransientResourceAllocator::CreateTexture(const FRHITextureCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex)
{
	FDynamicRHI::FRHICalcTextureSizeResult MemReq = GVulkanRHI->RHICalcTexturePlatformSize(InCreateInfo, 0);

	return CreateTextureInternal(InCreateInfo, InDebugName, InPassIndex, MemReq.Size, MemReq.Align,
		[&](const FRHITransientHeap::FResourceInitializer& Initializer)
	{
		FRHITextureCreateDesc CreateDesc(InCreateInfo, ERHIAccess::Discard, InDebugName);
		FRHITexture* Texture = new FVulkanTexture(*Device, CreateDesc, &Initializer.Allocation);
		return new FRHITransientTexture(Texture, 0/*GpuVirtualAddress*/, Initializer.Hash, MemReq.Size, ERHITransientAllocationType::Heap, InCreateInfo);
	});
}

FRHITransientBuffer* FVulkanTransientResourceAllocator::CreateBuffer(const FRHIBufferCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex)
{
	checkf(!EnumHasAnyFlags(InCreateInfo.Usage, BUF_AccelerationStructure), TEXT("AccelerationStructure not yet supported as TransientResource."));
	checkf(!EnumHasAnyFlags(InCreateInfo.Usage, BUF_Volatile), TEXT("The volatile flag is not supported for transient resources."));

	const bool bZeroSize = (InCreateInfo.Size == 0);
	const uint32 Alignment = FMemoryManager::CalculateBufferAlignment(*Device, InCreateInfo.Usage, bZeroSize);
	uint64 Size = Align(InCreateInfo.Size, Alignment);

	return CreateBufferInternal(InCreateInfo, InDebugName, InPassIndex, Size, Alignment,
		[&](const FRHITransientHeap::FResourceInitializer& Initializer)
	{
		FRHIResourceCreateInfo ResourceCreateInfo(InDebugName);
		FRHIBuffer* Buffer = new FVulkanResourceMultiBuffer(Device, FRHIBufferDesc(InCreateInfo.Size, InCreateInfo.Stride, InCreateInfo.Usage), ResourceCreateInfo, nullptr, &Initializer.Allocation);
		Buffer->SetTrackedAccess_Unsafe(ERHIAccess::Discard);
		return new FRHITransientBuffer(Buffer, 0/*GpuVirtualAddress*/, Initializer.Hash, Size, ERHITransientAllocationType::Heap, InCreateInfo);
	});
}
