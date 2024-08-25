// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VulkanResources.h"
#include "RHICoreTransientResourceAllocator.h"


class FVulkanTransientHeap final
	: public FRHITransientHeap
	, public FRefCountBase
	, public FDeviceChild
{
public:
	FVulkanTransientHeap(const FInitializer& Initializer, FVulkanDevice* InDevice);
	~FVulkanTransientHeap();

	VkDeviceMemory GetMemoryHandle();
	static FVulkanAllocation GetVulkanAllocation(const FRHITransientHeapAllocation& TransientInitializer);

private:
	VkBuffer VulkanBuffer;
	VkMemoryRequirements MemoryRequirements;
	FVulkanAllocation InternalAllocation;
};


class FVulkanTransientHeapCache final
	: public FRHITransientHeapCache
	, public FDeviceChild
{
public:
	static FVulkanTransientHeapCache* Create(FVulkanDevice* InDevice);

	FRHITransientHeap* CreateHeap(const FRHITransientHeap::FInitializer& Initializer) override;

private:
	FVulkanTransientHeapCache(const FInitializer& Initializer, FVulkanDevice* InDevice);
};


class FVulkanTransientResourceAllocator final
	: public FRHITransientResourceHeapAllocator
	, public FDeviceChild
{
public:
	FVulkanTransientResourceAllocator(FVulkanTransientHeapCache& InHeapCache);

	//! IRHITransientResourceAllocator Overrides
	bool SupportsResourceType(ERHITransientResourceType InType) const override
	{
		switch (InType)
		{
		case ERHITransientResourceType::Buffer: return true;
		case ERHITransientResourceType::Texture: return true;
		default: checkNoEntry(); return false;
		}
	}
	FRHITransientTexture* CreateTexture(const FRHITextureCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) override;
	FRHITransientBuffer* CreateBuffer(const FRHIBufferCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) override;
};