// Copyright Epic Games, Inc. All Rights Reserved..

#pragma once

#include "CoreMinimal.h"
#include "VulkanCommon.h"


struct FVulkanPipelineBarrier
{
	FVulkanPipelineBarrier() : SrcStageMask(0), DstStageMask(0), Semaphore(nullptr), bHasMemoryBarrier(false)
	{
		ZeroVulkanStruct(MemoryBarrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
	}

	ERHIPipeline SrcPipelines, DstPipelines;
	VkPipelineStageFlags SrcStageMask, DstStageMask;
	VkMemoryBarrier MemoryBarrier;
	TArray<VkImageMemoryBarrier, TInlineAllocator<2>> ImageBarriers;
	TArray<VkBufferMemoryBarrier> BufferBarriers;
	VulkanRHI::FSemaphore* Semaphore;
	bool bHasMemoryBarrier;

	// We need to keep the texture pointers around, because we need to call OnTransitionResource on them, and we need mip and layer counts for the tracking code.
	struct ImageBarrierExtraData
	{
		FVulkanTexture* BaseTexture = nullptr;
		bool IsAliasingBarrier = false;
	};
	TArray<ImageBarrierExtraData, TInlineAllocator<2>> ImageBarrierExtras;

	void AddMemoryBarrier(VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask);
	void AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresourceRange);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const struct FVulkanImageLayout& SrcLayout, VkImageLayout DstLayout);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout SrcLayout, const struct FVulkanImageLayout& DstLayout);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const struct FVulkanImageLayout& SrcLayout, const struct FVulkanImageLayout& DstLayout);
	void AddImageAccessTransition(const FVulkanTexture& Surface, ERHIAccess SrcAccess, ERHIAccess DstAccess, const VkImageSubresourceRange& SubresourceRange, VkImageLayout& InOutLayout);
	void Execute(VkCommandBuffer CmdBuffer);

	static VkImageSubresourceRange MakeSubresourceRange(VkImageAspectFlags AspectMask, uint32 FirstMip = 0, uint32 NumMips = VK_REMAINING_MIP_LEVELS, uint32 FirstLayer = 0, uint32 NumLayers = VK_REMAINING_ARRAY_LAYERS);
};

struct FVulkanImageLayout
{
	FVulkanImageLayout(VkImageLayout InitialLayout, uint32 InNumMips, uint32 InNumLayers) :
		NumMips(InNumMips),
		NumLayers(InNumLayers),
		MainLayout(InitialLayout)
	{
	}

	uint32 NumMips;
	uint32 NumLayers;

	// The layout when all the subresources are in the same state.
	VkImageLayout MainLayout;

	// Explicit subresource layouts. Always NumLayers*NumMips elements.
	TArray<VkImageLayout> SubresLayouts;

	bool AreAllSubresourcesSameLayout() const
	{
		return SubresLayouts.Num() == 0;
	}

	VkImageLayout GetSubresLayout(uint32 Layer, uint32 Mip) const
	{
		if (SubresLayouts.Num() == 0)
		{
			return MainLayout;
		}

		if (Layer == (uint32)-1)
		{
			Layer = 0;
		}

		check(Layer < NumLayers && Mip < NumMips);
		return SubresLayouts[Layer * NumMips + Mip];
	}

	bool AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange) const;

	uint32 GetSubresRangeLayerCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		return (SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) ? NumLayers : SubresourceRange.layerCount;
	}

	uint32 GetSubresRangeMipCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		return (SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) ? NumMips : SubresourceRange.levelCount;
	}

	void CollapseSubresLayoutsIfSame();

	void Set(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange);
};

class FVulkanLayoutManager
{
public:
	FVulkanLayoutManager()
		: CurrentRenderPass(nullptr)
		, CurrentFramebuffer(nullptr)
	{
	}

	void TempCopy(const FVulkanLayoutManager& In)
	{
		Framebuffers = In.Framebuffers;
		RenderPasses = In.RenderPasses;
		Layouts = In.Layouts;
	}

	void Destroy(FVulkanDevice& InDevice, FVulkanLayoutManager* Immediate);

	FVulkanFramebuffer* GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass);
	FVulkanRenderPass* GetOrCreateRenderPass(FVulkanDevice& InDevice, const FVulkanRenderTargetLayout& RTLayout)
	{
		uint32 RenderPassHash = RTLayout.GetRenderPassFullHash();
		FVulkanRenderPass** FoundRenderPass = nullptr;
		{
			FScopeLock Lock(&RenderPassesCS);
			FoundRenderPass = RenderPasses.Find(RenderPassHash);
		}
		if (FoundRenderPass)
		{
			return *FoundRenderPass;
		}

		FVulkanRenderPass* RenderPass = new FVulkanRenderPass(InDevice, RTLayout);
		{
			FScopeLock Lock(&RenderPassesCS);
			FoundRenderPass = RenderPasses.Find(RenderPassHash);
			if (FoundRenderPass)
			{
				delete RenderPass;
				return *FoundRenderPass;
			}
			RenderPasses.Add(RenderPassHash, RenderPass);
		}
		return RenderPass;
	}

	void BeginRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHIRenderPassInfo& RPInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer);
	void EndRenderPass(FVulkanCmdBuffer* CmdBuffer);

	FVulkanRenderPass* CurrentRenderPass;
	FVulkanFramebuffer* CurrentFramebuffer;

	FCriticalSection RenderPassesCS;

	void NotifyDeletedRenderTarget(FVulkanDevice& InDevice, VkImage Image);
	void NotifyDeletedImage(VkImage Image);

	FVulkanImageLayout* GetFullLayout(VkImage Image)
	{
		return Layouts.Find(Image);
	}

	FVulkanImageLayout& GetFullLayoutChecked(VkImage Image)
	{
		return Layouts.FindChecked(Image);
	}

	FVulkanImageLayout& GetOrAddFullLayout(const FVulkanTexture& Surface, VkImageLayout LayoutIfNotFound)
	{
		FVulkanImageLayout* Layout = Layouts.Find(Surface.Image);
		if (Layout)
		{
			return *Layout;
		}

		return Layouts.Add(Surface.Image, FVulkanImageLayout(LayoutIfNotFound, Surface.GetNumMips(), Surface.GetNumberOfArrayLevels()));
	}

	//
	// The following functions should only be used when the calling code is sure that the image has all its sub-resources
	// in the same state. They will assert if that's not the case.
	//
	VkImageLayout FindLayoutChecked(VkImage Image) const
	{
		const FVulkanImageLayout& Layout = Layouts.FindChecked(Image);
		check(Layout.AreAllSubresourcesSameLayout());
		return Layout.MainLayout;
	}

	VULKANRHI_API FVulkanImageLayout& FindOrAddFullLayoutRW(VkImage Image, VkImageLayout LayoutIfNotFound, uint32 NumMips, uint32 NumLayers)
	{
		FVulkanImageLayout* Layout = Layouts.Find(Image);
		if (Layout)
		{
			check(Layout->AreAllSubresourcesSameLayout());
			return *Layout;
		}
		return Layouts.Add(Image, FVulkanImageLayout(LayoutIfNotFound, NumMips, NumLayers));
	}

	VULKANRHI_API VkImageLayout& FindOrAddLayoutRW(VkImage Image, VkImageLayout LayoutIfNotFound, uint32 NumMips, uint32 NumLayers)
	{
		return FindOrAddFullLayoutRW(Image, LayoutIfNotFound, NumMips, NumLayers).MainLayout;
	}

	VULKANRHI_API VkImageLayout& FindOrAddLayoutRW(const FVulkanTexture& Surface, VkImageLayout LayoutIfNotFound)
	{
		return FindOrAddLayoutRW(Surface.Image, LayoutIfNotFound, Surface.GetNumMips(), Surface.GetNumberOfArrayLevels());
	}

	VkImageLayout FindOrAddLayout(const FVulkanTexture& Surface, VkImageLayout LayoutIfNotFound)
	{
		return FindOrAddLayoutRW(Surface, LayoutIfNotFound);
	}

private:
	TMap<uint32, FVulkanRenderPass*> RenderPasses;

	struct FFramebufferList
	{
		TArray<FVulkanFramebuffer*> Framebuffer;
	};
	TMap<uint32, FFramebufferList*> Framebuffers;

	TMap<VkImage, FVulkanImageLayout> Layouts;

	void ValidateRenderPassColorEntry(const FRHIRenderPassInfo::FColorEntry& ColorEntry, bool bResolveTarget, FVulkanPipelineBarrier& Barrier);
};
