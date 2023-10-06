// Copyright Epic Games, Inc. All Rights Reserved..

#pragma once

#include "CoreMinimal.h"
#include "VulkanCommon.h"

class FVulkanCmdBuffer;

struct FVulkanPipelineBarrier
{
	FVulkanPipelineBarrier() : Semaphore(nullptr)
	{}

	using MemoryBarrierArrayType = TArray<VkMemoryBarrier2, TInlineAllocator<1>>;
	using ImageBarrierArrayType = TArray<VkImageMemoryBarrier2, TInlineAllocator<2>>;
	using BufferBarrierArrayType = TArray<VkBufferMemoryBarrier2>;

	ERHIPipeline SrcPipelines, DstPipelines;
	MemoryBarrierArrayType MemoryBarriers;
	ImageBarrierArrayType ImageBarriers;
	BufferBarrierArrayType BufferBarriers;
	VulkanRHI::FSemaphore* Semaphore;

	// We need to keep the texture pointers around, because we need to call OnTransitionResource on them, and we need mip and layer counts for the tracking code.
	struct ImageBarrierExtraData
	{
		FVulkanTexture* BaseTexture = nullptr;
		bool IsAliasingBarrier = false;
	};
	TArray<ImageBarrierExtraData, TInlineAllocator<2>> ImageBarrierExtras;

	void AddMemoryBarrier(VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask);
	void AddFullImageLayoutTransition(const FVulkanTexture& Texture, VkImageLayout SrcLayout, VkImageLayout DstLayout);
	void AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresourceRange);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const struct FVulkanImageLayout& SrcLayout, VkImageLayout DstLayout);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout SrcLayout, const struct FVulkanImageLayout& DstLayout);
	void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const struct FVulkanImageLayout& SrcLayout, const struct FVulkanImageLayout& DstLayout);
	void AddImageAccessTransition(const FVulkanTexture& Surface, ERHIAccess SrcAccess, ERHIAccess DstAccess, const VkImageSubresourceRange& SubresourceRange, VkImageLayout& InOutLayout);
	void Execute(VkCommandBuffer CmdBuffer);
	void Execute(FVulkanCmdBuffer* CmdBuffer);

	static VkImageSubresourceRange MakeSubresourceRange(VkImageAspectFlags AspectMask, uint32 FirstMip = 0, uint32 NumMips = VK_REMAINING_MIP_LEVELS, uint32 FirstLayer = 0, uint32 NumLayers = VK_REMAINING_ARRAY_LAYERS);
};

struct FVulkanImageLayout
{
	FVulkanImageLayout(VkImageLayout InitialLayout, uint32 InNumMips, uint32 InNumLayers, VkImageAspectFlags Aspect) :
		NumMips(InNumMips),
		NumLayers(InNumLayers),
		NumPlanes((Aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ? 2 : 1),
		MainLayout(InitialLayout)
	{
	}

	uint32 NumMips;
	uint32 NumLayers;
	uint32 NumPlanes;

	// The layout when all the subresources are in the same state.
	VkImageLayout MainLayout;

	// Explicit subresource layouts. Always NumLayers*NumMips elements.
	TArray<VkImageLayout> SubresLayouts;

	inline bool AreAllSubresourcesSameLayout() const
	{
		return SubresLayouts.Num() == 0;
	}

	VkImageLayout GetSubresLayout(uint32 Layer, uint32 Mip, VkImageAspectFlagBits Aspect) const
	{
		return GetSubresLayout(Layer, Mip, (Aspect==VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0);
	}

	VkImageLayout GetSubresLayout(uint32 Layer, uint32 Mip, uint32 Plane) const
	{
		if (SubresLayouts.Num() == 0)
		{
			return MainLayout;
		}

		if (Layer == (uint32)-1)
		{
			Layer = 0;
		}

		check(Plane < NumPlanes && Layer < NumLayers && Mip < NumMips);
		return SubresLayouts[(Plane * NumLayers * NumMips) + (Layer * NumMips) + Mip];
	}

	bool AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange) const;

	inline uint32 GetSubresRangeLayerCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		check(SubresourceRange.baseArrayLayer < NumLayers);
		return (SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) ? (NumLayers - SubresourceRange.baseArrayLayer) : SubresourceRange.layerCount;
	}

	inline uint32 GetSubresRangeMipCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		check(SubresourceRange.baseMipLevel < NumMips);
		return (SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) ? (NumMips - SubresourceRange.baseMipLevel) : SubresourceRange.levelCount;
	}

	void CollapseSubresLayoutsIfSame();

	void Set(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange);
};

class FVulkanLayoutManager
{
public:
	FVulkanLayoutManager(bool InWriteOnly, FVulkanLayoutManager* InFallback)
		: bWriteOnly(InWriteOnly)
		, Fallback(InFallback)
	{
	}

	void NotifyDeletedImage(VkImage Image);

	// Predetermined layouts for given RHIAccess
	static VkImageLayout GetDefaultLayout(FVulkanCmdBuffer* CmdBuffer, const FVulkanTexture& VulkanTexture, ERHIAccess DesiredAccess);

	// Expected layouts and Hints are workarounds until we can use 'hardcoded layouts' everywhere.
	static VkImageLayout SetExpectedLayout(FVulkanCmdBuffer* CmdBuffer, const FVulkanTexture& VulkanTexture, ERHIAccess DesiredAccess);
	VkImageLayout GetDepthStencilHint(const FVulkanTexture& VulkanTexture, VkImageAspectFlagBits AspectBit);

	VULKANRHI_API const FVulkanImageLayout* GetFullLayout(VkImage Image) const
	{
		check(!bWriteOnly);
		const FVulkanImageLayout* Layout = Layouts.Find(Image);
		if (!Layout && Fallback)
		{
			return Fallback->GetFullLayout(Image);
		}
		return Layout;
	}

	VULKANRHI_API const FVulkanImageLayout* GetFullLayout(const FVulkanTexture& VulkanTexture, bool bAddIfNotFound = false, VkImageLayout LayoutIfNotFound = VK_IMAGE_LAYOUT_UNDEFINED)
	{
		check(!bWriteOnly);
		const FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture.Image);
		
		if (!Layout && Fallback)
		{
			Layout = Fallback->GetFullLayout(VulkanTexture, false);

			// If the layout was found in the fallback, carry it forward to our current manager for future tracking
			if (Layout)
			{
				return &Layouts.Add(VulkanTexture.Image, *Layout);
			}
		}
		
		if (Layout)
		{
			return Layout;
		}
		else if (!bAddIfNotFound)
		{
			return nullptr;
		}

		return &Layouts.Add(VulkanTexture.Image, FVulkanImageLayout(LayoutIfNotFound, VulkanTexture.GetNumMips(), VulkanTexture.GetNumberOfArrayLevels(), VulkanTexture.GetFullAspectMask()));
	}

	// Not the preferred path because we can't ensure Mip and Layer counts match, but still necessary for images like the backbuffer
	VULKANRHI_API void SetFullLayout(VkImage Image, const FVulkanImageLayout& NewLayout)
	{
		FVulkanImageLayout* Layout = Layouts.Find(Image);
		if (Layout)
		{
			*Layout = NewLayout;
		}
		else
		{
			Layouts.Add(Image, NewLayout);
		}
	}

	VULKANRHI_API void SetFullLayout(const FVulkanTexture& VulkanTexture, const FVulkanImageLayout& NewLayout)
	{
		check((VulkanTexture.GetNumMips() == NewLayout.NumMips) && (VulkanTexture.GetNumberOfArrayLevels() == NewLayout.NumLayers));
		SetFullLayout(VulkanTexture.Image, NewLayout);
	}

	VULKANRHI_API void SetFullLayout(const FVulkanTexture& VulkanTexture, VkImageLayout InLayout, bool bOnlyIfNotFound=false)
	{
		FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture.Image);
		if (Layout)
		{
			if (!bOnlyIfNotFound)
			{
				Layout->Set(InLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VulkanTexture.GetFullAspectMask()));
			}
		}
		else
		{
			Layouts.Add(VulkanTexture.Image, FVulkanImageLayout(InLayout, VulkanTexture.GetNumMips(), VulkanTexture.GetNumberOfArrayLevels(), VulkanTexture.GetFullAspectMask()));
		}
	}

	VULKANRHI_API void SetLayout(const FVulkanTexture& VulkanTexture, const VkImageSubresourceRange& InSubresourceRange, VkImageLayout InLayout)
	{
		FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture.Image);
		if (Layout)
		{
			Layout->Set(InLayout, InSubresourceRange);
		}
		else
		{
			FVulkanImageLayout NewLayout(VK_IMAGE_LAYOUT_UNDEFINED, VulkanTexture.GetNumMips(), VulkanTexture.GetNumberOfArrayLevels(), VulkanTexture.GetFullAspectMask());
			NewLayout.Set(InLayout, InSubresourceRange);
			Layouts.Add(VulkanTexture.Image, NewLayout);
		}
	}

	// Transfers our layouts into the destination
	void TransferTo(FVulkanLayoutManager& Destination);

private:

	TMap<VkImage, FVulkanImageLayout> Layouts;

	// If we're WriteOnly, we should never read layout from this instance.  This is important for parallel rendering.
	// When WriteOnly, this instance of the layout manager should only collect layouts to later feed them to the another central mgr.
	const bool bWriteOnly;

	// If parallel command list creation is NOT supported, then the queue's layout mgr can be used as a fallback to fetch previous layouts.
	FVulkanLayoutManager* Fallback;
};
