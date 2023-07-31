// Copyright Epic Games, Inc. All Rights Reserved..

#include "VulkanRHIPrivate.h"
#include "VulkanBarriers.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"

FVulkanLayoutManager FVulkanCommandListContext::LayoutManager;

// All shader stages supported by VK device - VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, FRAGMENT etc
extern uint32 GVulkanDevicePipelineStageBits;

//
// The following two functions are used when the RHI needs to do image layout transitions internally.
// They are not used for the transitions requested through the public API (RHICreate/Begin/EndTransition)
// unless the initial state in ERHIAccess::Unknown, in which case the tracking code kicks in.
//
static VkAccessFlags GetVkAccessMaskForLayout(VkImageLayout Layout)
{
	VkAccessFlags Flags = 0;

	switch (Layout)
	{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			Flags = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			Flags = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			Flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
#if VULKAN_SUPPORTS_SEPARATE_DEPTH_STENCIL_LAYOUTS
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR:
#endif
			Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
			Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
#if VULKAN_SUPPORTS_SEPARATE_DEPTH_STENCIL_LAYOUTS
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR:
#endif
			Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = 0;
			break;

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
			Flags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
			break;
#endif

#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			Flags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			break;
#endif

		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = 0;
			break;

		default:
			checkNoEntry();
			break;
	}

	return Flags;
}

static VkPipelineStageFlags GetVkStageFlagsForLayout(VkImageLayout Layout)
{
	VkPipelineStageFlags Flags = 0;

	switch (Layout)
	{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
#if VULKAN_SUPPORTS_SEPARATE_DEPTH_STENCIL_LAYOUTS
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR:
#endif
			Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
#if VULKAN_SUPPORTS_SEPARATE_DEPTH_STENCIL_LAYOUTS
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR:
		case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR:
#endif
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
			break;
#endif

#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			break;
#endif
			
		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;

		default:
			checkNoEntry();
			break;
	}

	return Flags;
}

//
// Get the Vulkan stage flags, access flags and image layout (if relevant) corresponding to an ERHIAccess value from the public API.
//
static void GetVkStageAndAccessFlags(ERHIAccess RHIAccess, FRHITransitionInfo::EType ResourceType, uint32 UsageFlags, bool bIsDepthStencil, bool bSupportsReadOnlyOptimal, VkPipelineStageFlags& StageFlags, VkAccessFlags& AccessFlags, VkImageLayout& Layout, bool bIsSourceState)
{
	// From Vulkan's point of view, when performing a multisample resolve via a render pass attachment, resolve targets are the same as render targets .
	// The caller signals this situation by setting both the RTV and ResolveDst flags, and we simply remove ResolveDst in that case,
	// to treat the resource as a render target.
	const ERHIAccess ResolveAttachmentAccess = (ERHIAccess)(ERHIAccess::RTV | ERHIAccess::ResolveDst);
	if (RHIAccess == ResolveAttachmentAccess)
	{
		RHIAccess = ERHIAccess::RTV;
	}

#if VULKAN_RHI_RAYTRACING
	// BVHRead state may be combined with SRV, but we always treat this as just BVHRead by clearing the SRV mask
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::BVHRead))
	{
		RHIAccess &= ~ERHIAccess::SRVMask;
	}
#endif

	Layout = VK_IMAGE_LAYOUT_UNDEFINED;

	// The layout to use if SRV access is requested. In case of depth/stencil buffers, we don't need to worry about different states for the separate aspects, since that's handled explicitly elsewhere,
	// and this function is never called for depth-only or stencil-only transitions.
	const VkImageLayout SRVLayout = 
		bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : 
		bSupportsReadOnlyOptimal ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;

	// States which cannot be combined.
	switch (RHIAccess)
	{
		case ERHIAccess::Discard:
			// FIXME: Align with Unknown for now, this could perhaps be less brutal
			StageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			AccessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			Layout = bIsSourceState ? VK_IMAGE_LAYOUT_UNDEFINED : SRVLayout;
			return;

		case ERHIAccess::Unknown:
			// We don't know where this is coming from, so we'll stall everything.
			StageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			AccessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			return;

		case ERHIAccess::CPURead:
			// FIXME: is this correct?
			StageFlags = VK_PIPELINE_STAGE_HOST_BIT;
			AccessFlags = VK_ACCESS_HOST_READ_BIT;
			Layout = VK_IMAGE_LAYOUT_GENERAL;
			return;

		case ERHIAccess::Present:
			StageFlags = bIsSourceState ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			AccessFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			return;

		case ERHIAccess::RTV:
			StageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			AccessFlags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			return;

		case ERHIAccess::CopyDest:
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			AccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			return;

		case ERHIAccess::ResolveDst:
			// Used when doing a resolve via RHICopyToResolveTarget. For us, it's the same as CopyDst.
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			AccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			return;

#if VULKAN_RHI_RAYTRACING
		// vkrt todo: Finer grain stage flags would be ideal here.
		case ERHIAccess::BVHRead:
			StageFlags = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			AccessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			return;

		case ERHIAccess::BVHWrite:
			StageFlags = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			AccessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			return;
#endif // VULKAN_RHI_RAYTRACING
	}

	// If DSVWrite is set, we ignore everything else because it decides the layout.
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVWrite))
	{
		check(bIsDepthStencil);
		StageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		return;
	}

	// The remaining flags can be combined.
	StageFlags = 0;
	AccessFlags = 0;
	uint32 ProcessedRHIFlags = 0;

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::IndirectArgs))
	{
		check(ResourceType != FRHITransitionInfo::EType::Texture);
		StageFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		AccessFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

		ProcessedRHIFlags |= (uint32)ERHIAccess::IndirectArgs;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::VertexOrIndexBuffer))
	{
		check(ResourceType != FRHITransitionInfo::EType::Texture);
		StageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		switch (ResourceType)
		{
			case FRHITransitionInfo::EType::Buffer:
				if ((UsageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
				{
					AccessFlags |= VK_ACCESS_INDEX_READ_BIT;
				}
				if ((UsageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
				{
					AccessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				}
				break;
			default:
				checkNoEntry();
				break;
		}

		ProcessedRHIFlags |= (uint32)ERHIAccess::VertexOrIndexBuffer;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVRead))
	{
		check(bIsDepthStencil);
		StageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

		// If any of the SRV flags is set, the code below will set Layout to SRVLayout again, but it's fine since
		// SRVLayout takes into account bIsDepthStencil and ends up being the same as what we set here.
		Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVRead;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVGraphics))
	{
		StageFlags |= (GVulkanDevicePipelineStageBits & ~VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		Layout = SRVLayout;

		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVGraphics;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		// There are cases where we ping-pong images between UAVCompute and SRVCompute. In that case it may be more efficient to leave the image in VK_IMAGE_LAYOUT_GENERAL
		// (at the very least, it will mean fewer image barriers). There's no good way to detect this though, so it might be better if the high level code just did UAV
		// to UAV transitions in that case, instead of SRV <-> UAV.
		Layout = SRVLayout;

		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVCompute;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::UAVGraphics))
	{
		StageFlags |= (GVulkanDevicePipelineStageBits & ~VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		Layout = VK_IMAGE_LAYOUT_GENERAL;

		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVGraphics;
	}
			
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::UAVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		Layout = VK_IMAGE_LAYOUT_GENERAL;

		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVCompute;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::CopySrc | ERHIAccess::ResolveSrc))
	{
		// ResolveSrc is used when doing a resolve via RHICopyToResolveTarget. For us, it's the same as CopySrc.
		StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
		AccessFlags = VK_ACCESS_TRANSFER_READ_BIT;
		if (ResourceType == FRHITransitionInfo::EType::Texture)
		{
			// If this is requested for a texture, make sure it's not combined with other access flags which require a different layout. It's important
			// that this block is last, so that if any other flags set the layout before, we trigger the assert below.
			check(Layout == VK_IMAGE_LAYOUT_UNDEFINED);
			Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}

		ProcessedRHIFlags |= (uint32)(ERHIAccess::CopySrc | ERHIAccess::ResolveSrc);
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::ShadingRateSource) && ValidateShadingRateDataType())
	{
		checkf(ResourceType == FRHITransitionInfo::EType::Texture, TEXT("A non-texture resource was tagged as a shading rate source; only textures (Texture2D and Texture2DArray) can be used for this purpose."));
		
#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			AccessFlags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			Layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}
#endif

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
			AccessFlags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
			Layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}
#endif

		ProcessedRHIFlags |= (uint32)ERHIAccess::ShadingRateSource;
	}

	uint32 RemainingFlags = (uint32)RHIAccess & (~ProcessedRHIFlags);
	ensureMsgf(RemainingFlags == 0, TEXT("Some access flags were not processed. RHIAccess=%x, ProcessedRHIFlags=%x, RemainingFlags=%x"), RHIAccess, ProcessedRHIFlags, RemainingFlags);
}

//
// Helpers for merging separate depth-stencil transitions into a single transition.
//
struct FDepthStencilSubresTransition
{
	FVulkanTexture* Texture;
	int ArraySlice;
	ERHIAccess SrcDepthAccess;
	ERHIAccess DestDepthAccess;
	ERHIAccess SrcStencilAccess;
	ERHIAccess DestStencilAccess;
	int bDepthAccessSet : 1;
	int bStencilAccessSet : 1;
	int bAliasingBarrier : 1;
};

static void GetDepthStencilStageAndAccessFlags(ERHIAccess DepthAccess, ERHIAccess StencilAccess, VkPipelineStageFlags& StageFlags, VkAccessFlags& AccessFlags, VkImageLayout& Layout, bool bIsSourceState)
{
	if (DepthAccess == ERHIAccess::Unknown || StencilAccess == ERHIAccess::Unknown)
	{
		StageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		AccessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		return;
	}

	if (DepthAccess == ERHIAccess::Discard && StencilAccess == ERHIAccess::Discard)
	{
		StageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		AccessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		Layout = bIsSourceState ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		return;
	}

	Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	if (EnumHasAnyFlags(DepthAccess, ERHIAccess::CopySrc))
	{
		Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if (EnumHasAnyFlags(DepthAccess, ERHIAccess::CopyDest))
	{
		Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if (EnumHasAnyFlags(DepthAccess, ERHIAccess::DSVWrite))
	{
		if (EnumHasAnyFlags(StencilAccess, ERHIAccess::DSVWrite))
		{
			Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			Layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
		}
	}
	else
	{
		if (EnumHasAnyFlags(StencilAccess, ERHIAccess::DSVWrite))
		{
			Layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
		}
	}

	StageFlags = 0;
	AccessFlags = 0;
	ERHIAccess CombinedAccess = DepthAccess | StencilAccess;
	uint32 ProcessedRHIFlags = 0;

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::DSVWrite))
	{
		StageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVWrite;
	}
	
	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::DSVRead))
	{
		StageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVRead;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::SRVGraphics))
	{
		StageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVGraphics;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::UAVGraphics))
	{
		StageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVGraphics;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::SRVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVCompute;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::UAVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVCompute;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::CopySrc))
	{
		StageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		AccessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::CopySrc;
	}

	if (EnumHasAnyFlags(CombinedAccess, ERHIAccess::CopyDest))
	{
		StageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		AccessFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::CopyDest;
	}

	uint32 RemainingFlags = (uint32)CombinedAccess & (~ProcessedRHIFlags);
	ensureMsgf(RemainingFlags == 0, TEXT("Some access flags were not processed. DepthAccess=%x, StencilAccess=%x, ProcessedRHIFlags=%x, RemainingFlags=%x"), DepthAccess, StencilAccess, ProcessedRHIFlags, RemainingFlags);
}

//
// Helpers for filling in the fields of a VkImageMemoryBarrier structure.
//
static void SetupImageBarrier(VkImageMemoryBarrier& ImgBarrier, VkImage Image, VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresRange)
{
	ImgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ImgBarrier.pNext = nullptr;
	ImgBarrier.srcAccessMask = SrcAccessFlags;
	ImgBarrier.dstAccessMask = DstAccessFlags;
	ImgBarrier.oldLayout = SrcLayout;
	ImgBarrier.newLayout = DstLayout;
	ImgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImgBarrier.image = Image;
	ImgBarrier.subresourceRange = SubresRange;
}

static void SetupImageBarrierEntireRes(VkImageMemoryBarrier& ImgBarrier, VkImage Image, VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkImageLayout SrcLayout, VkImageLayout DstLayout, VkImageAspectFlags AspectMask)
{
	VkImageSubresourceRange SubresRange;
	SubresRange.aspectMask = AspectMask;
	SubresRange.baseMipLevel = 0;
	SubresRange.levelCount = VK_REMAINING_MIP_LEVELS;
	SubresRange.baseArrayLayer = 0;
	SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	SetupImageBarrier(ImgBarrier, Image, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresRange);
}

// Fill in a VkImageSubresourceRange struct from the data contained inside a transition info struct coming from the public API.
static void SetupSubresourceRange(VkImageSubresourceRange& SubresRange, const FRHITransitionInfo& TransitionInfo, VkImageAspectFlags AspectMask)
{
	SubresRange.aspectMask = AspectMask;
	if (TransitionInfo.IsAllMips())
	{
		SubresRange.baseMipLevel = 0;
		SubresRange.levelCount = VK_REMAINING_MIP_LEVELS;
	}
	else
	{
		SubresRange.baseMipLevel = TransitionInfo.MipIndex;
		SubresRange.levelCount = 1;
	}

	if (TransitionInfo.IsAllArraySlices())
	{
		SubresRange.baseArrayLayer = 0;
		SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	}
	else
	{
		SubresRange.baseArrayLayer = TransitionInfo.ArraySlice;
		SubresRange.layerCount = 1;
	}
}

void FVulkanDynamicRHI::RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo)
{
	const ERHIPipeline SrcPipelines = CreateInfo.SrcPipelines;
	const ERHIPipeline DstPipelines = CreateInfo.DstPipelines;

	//checkf(FMath::IsPowerOfTwo((uint32)SrcPipelines) && FMath::IsPowerOfTwo((uint32)DstPipelines), TEXT("Support for multi-pipe resources is not yet implemented."));

	FVulkanPipelineBarrier* Data = new (Transition->GetPrivateData<FVulkanPipelineBarrier>()) FVulkanPipelineBarrier;
	Data->SrcPipelines = SrcPipelines;
	Data->DstPipelines = DstPipelines;

	uint32 SrcQueueFamilyIndex, DstQueueFamilyIndex;
	if (SrcPipelines != DstPipelines)
	{
		Data->Semaphore = new VulkanRHI::FSemaphore(*Device);

		const uint32 GfxQueueIndex = Device->GetGraphicsQueue()->GetFamilyIndex();
		const uint32 ComputeQueueIndex = Device->GetComputeQueue()->GetFamilyIndex();

		if (SrcPipelines == ERHIPipeline::Graphics)
		{
			SrcQueueFamilyIndex = GfxQueueIndex;
			DstQueueFamilyIndex = ComputeQueueIndex;
		}
		else
		{
			SrcQueueFamilyIndex = ComputeQueueIndex;
			DstQueueFamilyIndex = GfxQueueIndex;
		}
	}
	else
	{
		SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	// Count the images and buffers to be able to pre-allocate the arrays.
	int32 NumTextures = 0, NumBuffers = 0;
	for (const FRHITransitionInfo& Info : CreateInfo.TransitionInfos)
	{
		if (!Info.Resource)
		{
			continue;
		}

		if (Info.AccessAfter == ERHIAccess::Discard)
		{
			// Discard as a destination is a no-op
			continue;
		}

		if (Info.Type == FRHITransitionInfo::EType::Texture)
		{
			// CPU accessible "textures" are implemented as buffers. Check if this is a real texture or a buffer.
			FVulkanTexture* Texture = FVulkanTexture::Cast(Info.Texture);
			if (Texture->GetCpuReadbackBuffer() == nullptr)
			{
				++NumTextures;
			}
			continue;
		}

		if (Info.Type == FRHITransitionInfo::EType::UAV)
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(Info.UAV);
			if (UAV->SourceTexture)
			{
				++NumTextures;
				continue;
			}
		}

		++NumBuffers;
	}

	Data->ImageBarriers.Reserve(NumTextures);
	Data->ImageBarrierExtras.Reserve(NumTextures);
	if (SrcPipelines != DstPipelines)
	{
		Data->BufferBarriers.Reserve(NumBuffers);
	}

	const ERHIAccess DepthStencilFlags = ERHIAccess::DSVRead | ERHIAccess::DSVWrite;

	TArray<FDepthStencilSubresTransition, TInlineAllocator<4>> DSSubresTransitions;

	for (const FRHITransitionInfo& Info : CreateInfo.TransitionInfos)
	{
		if (!Info.Resource)
		{
			continue;
		}

		if (Info.AccessAfter == ERHIAccess::Discard)
		{
			// Discard as a destination is a no-op
			continue;
		}

		checkf(Info.AccessAfter != ERHIAccess::Unknown, TEXT("Transitioning a resource to an unknown state is not allowed."));

		FVulkanResourceMultiBuffer* Buffer = nullptr;
		FVulkanTexture* Texture = nullptr;
		FRHITransitionInfo::EType UnderlyingType = Info.Type;
		uint32 UsageFlags = 0;

		switch (Info.Type)
		{
		case FRHITransitionInfo::EType::Texture:
		{
			Texture = FVulkanTexture::Cast(Info.Texture);
			if (Texture->GetCpuReadbackBuffer())
			{
				Texture = nullptr;
			}
			break;
		}

		case FRHITransitionInfo::EType::Buffer:
			Buffer = ResourceCast(Info.Buffer);
			UsageFlags = Buffer->GetBufferUsageFlags();
			break;

		case FRHITransitionInfo::EType::UAV:
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(Info.UAV);
			if (UAV->SourceTexture)
			{
				Texture = FVulkanTexture::Cast(UAV->SourceTexture);
				UnderlyingType = FRHITransitionInfo::EType::Texture;
			}
			else if (UAV->SourceBuffer)
			{
				Buffer = UAV->SourceBuffer;
				UnderlyingType = FRHITransitionInfo::EType::Buffer;
				UsageFlags = Buffer->GetBufferUsageFlags();
			}
			else
			{
				checkNoEntry();
				continue;
			}
			break;
		}

		case FRHITransitionInfo::EType::BVH:
		{
			// Requires memory barrier
			break;
		}

		default:
			checkNoEntry();
			continue;
		}

		VkPipelineStageFlags SrcStageMask, DstStageMask;
		VkAccessFlags SrcAccessFlags, DstAccessFlags;
		VkImageLayout SrcLayout, DstLayout;

		const bool bIsDepthStencil = Texture && Texture->IsDepthOrStencilAspect();

		// If the device doesn't support separate depth-stencil layouts, we must merge depth-stencil subresource transitions so we only do one barrier on the image.
		if (bIsDepthStencil && Info.PlaneSlice != FRHISubresourceRange::kAllSubresources)
		{
			int32 Index = DSSubresTransitions.IndexOfByPredicate([Texture, Info](const FDepthStencilSubresTransition& Entry) -> bool {
				return Entry.Texture->Image == Texture->Image && Entry.ArraySlice == Info.ArraySlice;
			});

			FDepthStencilSubresTransition* PendingTransition;
			if (Index == INDEX_NONE)
			{
				PendingTransition = &DSSubresTransitions.AddDefaulted_GetRef();
				PendingTransition->Texture = Texture;
				PendingTransition->bDepthAccessSet = PendingTransition->bStencilAccessSet = false;
				PendingTransition->ArraySlice = Info.ArraySlice;
			}
			else
			{
				PendingTransition = &DSSubresTransitions[Index];
			}

			if (Info.PlaneSlice == FRHISubresourceRange::kDepthPlaneSlice)
			{
				// We don't support multiple transitions on the same aspect.
				ensure(PendingTransition->bDepthAccessSet == false);
				PendingTransition->SrcDepthAccess = Info.AccessBefore;
				PendingTransition->DestDepthAccess = Info.AccessAfter;
				PendingTransition->bDepthAccessSet = true;
			}
			else
			{
				ensure(PendingTransition->bStencilAccessSet == false);
				PendingTransition->SrcStencilAccess = Info.AccessBefore;
				PendingTransition->DestStencilAccess = Info.AccessAfter;
				PendingTransition->bStencilAccessSet = true;
			}

			PendingTransition->bAliasingBarrier = (Info.AccessBefore == ERHIAccess::Discard);

			if (Index == INDEX_NONE)
			{
				// Wait until we find the other aspect of the resource.
				continue;
			}

			// Now that we have both aspect transitions filled in, we can figure out the layout.
			GetDepthStencilStageAndAccessFlags(PendingTransition->SrcDepthAccess, PendingTransition->SrcStencilAccess, SrcStageMask, SrcAccessFlags, SrcLayout, true);
			GetDepthStencilStageAndAccessFlags(PendingTransition->DestDepthAccess, PendingTransition->DestStencilAccess, DstStageMask, DstAccessFlags, DstLayout, false);

			// Remove the pending transition and add the barrier.
			DSSubresTransitions.RemoveAtSwap(Index);
		}
		else
		{
			
			const bool bSupportsReadOnlyOptimal = (Texture == nullptr) || Texture->SupportsSampling();

			GetVkStageAndAccessFlags(Info.AccessBefore, UnderlyingType, UsageFlags, bIsDepthStencil, bSupportsReadOnlyOptimal, SrcStageMask, SrcAccessFlags, SrcLayout, true);
			GetVkStageAndAccessFlags(Info.AccessAfter, UnderlyingType, UsageFlags, bIsDepthStencil, bSupportsReadOnlyOptimal, DstStageMask, DstAccessFlags, DstLayout, false);

			// If not compute, remove vertex pipeline bits as only compute updates vertex buffers
			if (!(SrcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))
			{
				DstStageMask &= ~(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
			}
		}

		// In case of async compute, override the stage and access flags computed above, since only a subset of stages is relevant.
		if (SrcPipelines == ERHIPipeline::AsyncCompute)
		{
			SrcStageMask = SrcStageMask & Device->GetComputeQueue()->GetSupportedStageBits();
			SrcAccessFlags = SrcAccessFlags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}

		if (DstPipelines == ERHIPipeline::AsyncCompute)
		{
			DstStageMask = DstStageMask & Device->GetComputeQueue()->GetSupportedStageBits();
			DstAccessFlags = DstAccessFlags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}


		// If we're not transitioning across pipes and we don't need to perform layout transitions, we can express memory dependencies through a global memory barrier.
		if ((SrcPipelines == DstPipelines) && (Texture == nullptr || SrcLayout == DstLayout))
		{
			Data->AddMemoryBarrier(SrcAccessFlags, DstAccessFlags, SrcStageMask, DstStageMask);
			continue;
		}

		// Add the stages affected by this transition.
		Data->SrcStageMask |= SrcStageMask;
		Data->DstStageMask |= DstStageMask;

		if (Buffer != nullptr)
		{
			// We only add buffer transitions for cross-pipe transfers.
			checkSlow( (SrcPipelines != DstPipelines) && (Texture == nullptr) );
			VkBufferMemoryBarrier& BufferBarrier = Data->BufferBarriers.AddDefaulted_GetRef();
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			BufferBarrier.pNext = nullptr;
			BufferBarrier.srcAccessMask = SrcAccessFlags;
			BufferBarrier.dstAccessMask = DstAccessFlags;
			BufferBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
			BufferBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;
			BufferBarrier.buffer = Buffer->GetHandle();
			BufferBarrier.offset = 0;
			BufferBarrier.size = VK_WHOLE_SIZE;
			continue;
		}

		check(Texture != nullptr);

		VkImageSubresourceRange SubresRange;
		SetupSubresourceRange(SubresRange, Info, Texture->GetFullAspectMask());

		// For some textures, e.g. FVulkanBackBuffer, the image handle may not be set yet, or may be stale, so there's no point storing it here.
		// We'll set the image to NULL in the barrier info, and RHIEndTransitions will fetch the up to date pointer from the texture, after
		// OnLayoutTransition is called.
		VkImageMemoryBarrier& ImgBarrier = Data->ImageBarriers.AddDefaulted_GetRef();
		SetupImageBarrier(ImgBarrier, VK_NULL_HANDLE, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresRange);
		ImgBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
		ImgBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;

		FVulkanPipelineBarrier::ImageBarrierExtraData& ExtraData = Data->ImageBarrierExtras.AddDefaulted_GetRef();
		ExtraData.BaseTexture = Texture;
		ExtraData.IsAliasingBarrier = (Info.AccessBefore == ERHIAccess::Discard);
	}

	// Process any depth-stencil transitions which specify a single sub-resource.
	for (const FDepthStencilSubresTransition& PendingTransition : DSSubresTransitions)
	{
		VkPipelineStageFlags SrcStageMask, DstStageMask;
		VkAccessFlags SrcAccessFlags, DstAccessFlags;
		VkImageLayout SrcLayout, DstLayout;

		auto GetOtherAspectAccess = [DepthStencilFlags](ERHIAccess ExplicitAspectAccess) -> ERHIAccess {
			// If the aspect that was explicitly set has any depth-stencil flags, we'll assume that the other aspect is read-only (DSVRead). When the barrier
			// is executed, we check the actual state of the other aspect and adjust the layout if it turns out that the assumption was wrong.
			// If we don't have any depth-stencil flags, we assume that both aspects will be in the same state, since it's presumably SRVGraphics or SRVCompute.
			return EnumHasAnyFlags(ExplicitAspectAccess, DepthStencilFlags) ? ERHIAccess::DSVRead : ExplicitAspectAccess;
		};

		const ERHIAccess CopyAccess = ERHIAccess::CopySrc | ERHIAccess::CopyDest;
		ERHIAccess SrcDepthAccess, SrcStencilAccess, DstDepthAccess, DstStencilAccess;
		VkImageAspectFlags AspectMask;
		if (PendingTransition.bDepthAccessSet)
		{
			SrcDepthAccess = PendingTransition.SrcDepthAccess;
			DstDepthAccess = PendingTransition.DestDepthAccess;
			SrcStencilAccess = GetOtherAspectAccess(SrcDepthAccess);
			DstStencilAccess = GetOtherAspectAccess(DstDepthAccess);
			AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			check(!EnumHasAnyFlags(PendingTransition.SrcDepthAccess, CopyAccess));
			check(!EnumHasAnyFlags(PendingTransition.DestDepthAccess, CopyAccess));
		}
		else
		{
			SrcStencilAccess = PendingTransition.SrcStencilAccess;
			DstStencilAccess = PendingTransition.DestStencilAccess;
			SrcDepthAccess = GetOtherAspectAccess(SrcStencilAccess);
			DstDepthAccess = GetOtherAspectAccess(DstStencilAccess);
			AspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

			check(!EnumHasAnyFlags(PendingTransition.SrcStencilAccess, CopyAccess));
			check(!EnumHasAnyFlags(PendingTransition.DestStencilAccess, CopyAccess));
		}

		GetDepthStencilStageAndAccessFlags(SrcDepthAccess, SrcStencilAccess, SrcStageMask, SrcAccessFlags, SrcLayout, true);
		GetDepthStencilStageAndAccessFlags(DstDepthAccess, DstStencilAccess, DstStageMask, DstAccessFlags, DstLayout, false);
		
		// Don't bother trying to figure out the source layout, let the cache fill it in.
		SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		Data->SrcStageMask |= SrcStageMask;
		Data->DstStageMask |= DstStageMask;

		VkImageMemoryBarrier& ImgBarrier = Data->ImageBarriers.AddDefaulted_GetRef();
		SetupImageBarrierEntireRes(ImgBarrier, VK_NULL_HANDLE, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, AspectMask);
		ImgBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
		ImgBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;

		FVulkanPipelineBarrier::ImageBarrierExtraData& ExtraData = Data->ImageBarrierExtras.AddDefaulted_GetRef();
		ExtraData.BaseTexture = PendingTransition.Texture;
		ExtraData.IsAliasingBarrier = PendingTransition.bAliasingBarrier;
	}
}

void FVulkanDynamicRHI::RHIReleaseTransition(FRHITransition* Transition)
{
	Transition->GetPrivateData<FVulkanPipelineBarrier>()->~FVulkanPipelineBarrier();
}

static void GetDepthStencilWritableState(VkImageLayout Layout, bool& bDepthWritable, bool& bStencilWritable)
{
	switch (Layout)
	{
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		bDepthWritable = true;
		bStencilWritable = true;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
		bDepthWritable = false;
		bStencilWritable = true;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
		bDepthWritable = true;
		bStencilWritable = false;
		break;

	default:
		// This includes VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, which is what we normally expect for read-only DS.
		bDepthWritable = false;
		bStencilWritable = false;
		break;
	}
}

static void AdjustDepthStencilLayout(VkImageMemoryBarrier& Barrier, VkImageAspectFlags FullAspectMask)
{
	// If this barrier specifies a sub-aspect of a depth-stencil surface, we need to make sure that the layout reflects the current state
	// of the other aspect. The code in RHICreateTransition cannot know the current state, so it assumed it's read-only when it set up the
	// barrier. If it turns out it's writable, we need to change the layout accordingly.
	if (FullAspectMask == Barrier.subresourceRange.aspectMask)
	{
		return;
	}

	if (!ensure(Barrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT || Barrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		return;
	}

	bool bDepthWritableCurrent, bStencilWritableCurrent;
	GetDepthStencilWritableState(Barrier.oldLayout, bDepthWritableCurrent, bStencilWritableCurrent);

	bool bDepthWritableNew, bStencilWritableNew;
	GetDepthStencilWritableState(Barrier.newLayout, bDepthWritableNew, bStencilWritableNew);

	if (Barrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		if (bStencilWritableCurrent)
		{
			Barrier.newLayout = bDepthWritableNew ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
		}
	}
	else
	{
		if (bDepthWritableCurrent)
		{
			Barrier.newLayout = bStencilWritableNew ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
		}
	}

	// Now that we have the correct layout, we can set the mask to include both aspects.
	Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
}

void FVulkanCommandListContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIBeginTransitions, bShowTransitionEvents, TEXT("RHIBeginTransitions"));

	TArray<VkBufferMemoryBarrier, TInlineAllocator<8>> RealBufferBarriers;
	TArray<VkImageMemoryBarrier, TInlineAllocator<8>> RealImageBarriers;
	TArray<VulkanRHI::FSemaphore*, TInlineAllocator<8>> SignalSemaphores;

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	for (const FRHITransition* Transition : Transitions)
	{
		const FVulkanPipelineBarrier* Data = Transition->GetPrivateData<FVulkanPipelineBarrier>();

		// We only care about cross-pipe transitions in this function.
		if (Data->SrcPipelines == Data->DstPipelines)
		{
			continue;
		}

		if (Data->SrcStageMask == 0 || Data->DstStageMask == 0)
		{
			// This transition didn't specify any resources.
			check(Data->ImageBarriers.Num() == 0 && Data->BufferBarriers.Num() == 0);
			continue;
		}

#if DO_GUARD_SLOW
		switch (Data->SrcPipelines)
		{
			case ERHIPipeline::Graphics:
				checkf(!Device->IsRealAsyncComputeContext(this), TEXT("Attempt to begin Graphics -> AsyncCompute transition on the async compute command list."));
				break;
			case ERHIPipeline::AsyncCompute:
				checkf(Device->IsRealAsyncComputeContext(this), TEXT("Attempt to begin AsyncCompute -> Graphics transition on the graphics command list."));
				break;
			default:
				checkNoEntry();
				break;
		}
#endif

		RealBufferBarriers.Reset();
		RealBufferBarriers.Reserve(Data->BufferBarriers.Num());
		for (const VkBufferMemoryBarrier& BufferBarrier : Data->BufferBarriers)
		{
			VkBufferMemoryBarrier& RealBarrier = RealBufferBarriers.AddDefaulted_GetRef();
			RealBarrier = BufferBarrier;
			RealBarrier.dstAccessMask = 0; // Release resource from current queue.
		}

		check(Data->ImageBarriers.Num() == Data->ImageBarrierExtras.Num());
		RealImageBarriers.Reset();
		RealImageBarriers.Reserve(Data->ImageBarriers.Num());

		VkPipelineStageFlags RealSrcStageMask = Data->SrcStageMask;
		VkPipelineStageFlags RealDstStageMask = Data->DstStageMask;

		for (int32 ImgBarrierIdx = 0; ImgBarrierIdx < Data->ImageBarriers.Num(); ++ImgBarrierIdx)
		{
			const VkImageMemoryBarrier& ImageBarrier = Data->ImageBarriers[ImgBarrierIdx];
			const FVulkanPipelineBarrier::ImageBarrierExtraData& ExtraData = Data->ImageBarrierExtras[ImgBarrierIdx];
			FVulkanTexture* Texture = ExtraData.BaseTexture;
			check(Texture->Image != VK_NULL_HANDLE);

			const FVulkanImageLayout& Layout = LayoutManager.GetOrAddFullLayout(*Texture, VK_IMAGE_LAYOUT_UNDEFINED);

			VkAccessFlags SrcAccessFlags;
			VkImageLayout SrcLayout, DstLayout;

			check(ImageBarrier.newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			DstLayout = ImageBarrier.newLayout;

			if ((ImageBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (!ExtraData.IsAliasingBarrier))
			{
				check(Layout.AreAllSubresourcesSameLayout());
				SrcLayout = Layout.MainLayout;
				SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
				RealSrcStageMask |= GetVkStageFlagsForLayout(SrcLayout);
			}
			else
			{
				checkSlow(Layout.AreSubresourcesSameLayout(ImageBarrier.oldLayout, ImageBarrier.subresourceRange));
				SrcLayout = ImageBarrier.oldLayout;
				SrcAccessFlags = ImageBarrier.srcAccessMask;
			}

			VkImageMemoryBarrier& RealBarrier = RealImageBarriers.AddDefaulted_GetRef();
			RealBarrier = ImageBarrier;
			RealBarrier.image = Texture->Image;
			RealBarrier.srcAccessMask = SrcAccessFlags;
			RealBarrier.dstAccessMask = 0; // Release resource from current queue.
			RealBarrier.oldLayout = SrcLayout;
			RealBarrier.newLayout = DstLayout;
			
			// Fix up the destination layout if this barrier specifies a sub-aspect of a depth-stencil 
			AdjustDepthStencilLayout(RealBarrier, Texture->GetFullAspectMask());

			// We don't update the image layout here. That will be done in RHIEndTransitions.
		}

		if (EnumHasAnyFlags(Data->SrcPipelines, ERHIPipeline::AsyncCompute))
		{
			RealSrcStageMask = RealSrcStageMask & Queue->GetSupportedStageBits();
		}

		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), RealSrcStageMask, RealDstStageMask, 0, 0, nullptr, RealBufferBarriers.Num(), RealBufferBarriers.GetData(), RealImageBarriers.Num(), RealImageBarriers.GetData());

		check(Data->Semaphore);
		SignalSemaphores.Add(Data->Semaphore);
	}

	if (SignalSemaphores.Num() > 0)
	{
		CommandBufferManager->SubmitActiveCmdBuffer(SignalSemaphores);
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
	}
}

static void AddSubresourceTransitions(TArray<VkImageMemoryBarrier, TInlineAllocator<8>>& Barriers, VkPipelineStageFlags& SrcStageMask, const VkImageMemoryBarrier& TemplateBarrier, VkImage ImageHandle, FVulkanImageLayout& CurrentLayout, VkImageLayout DstLayout)
{
	const uint32 FirstLayer = TemplateBarrier.subresourceRange.baseArrayLayer;
	const uint32 LastLayer = FirstLayer + CurrentLayout.GetSubresRangeLayerCount(TemplateBarrier.subresourceRange);
	
	const uint32 FirstMip = TemplateBarrier.subresourceRange.baseMipLevel;
	const uint32 LastMip = FirstMip + CurrentLayout.GetSubresRangeMipCount(TemplateBarrier.subresourceRange);

	for (uint32 LayerIdx = FirstLayer; LayerIdx < LastLayer; ++LayerIdx)
	{
		VkImageMemoryBarrier* PrevMipBarrier = nullptr;

		for (uint32 MipIdx = FirstMip; MipIdx < LastMip; ++MipIdx)
		{
			VkImageLayout SrcLayout = CurrentLayout.GetSubresLayout(LayerIdx, MipIdx);

			// Merge with the previous transition if the previous mip was in the same state as this mip.
			if (PrevMipBarrier && PrevMipBarrier->oldLayout == SrcLayout)
			{
				PrevMipBarrier->subresourceRange.levelCount += 1;
			}
			else
			{
				if (SrcLayout == DstLayout)
				{
					continue;
				}

				SrcStageMask |= GetVkStageFlagsForLayout(SrcLayout);

				VkImageMemoryBarrier& Barrier = Barriers.AddDefaulted_GetRef();
				Barrier = TemplateBarrier;
				Barrier.srcAccessMask = GetVkAccessMaskForLayout(SrcLayout);
				Barrier.oldLayout = SrcLayout;
				Barrier.newLayout = DstLayout;
				Barrier.image = ImageHandle;
				Barrier.subresourceRange.baseMipLevel = MipIdx;
				Barrier.subresourceRange.levelCount = 1;
				Barrier.subresourceRange.baseArrayLayer = LayerIdx;
				Barrier.subresourceRange.layerCount = 1;

				PrevMipBarrier = &Barrier;
			}
		}
	}

	CurrentLayout.Set(DstLayout, TemplateBarrier.subresourceRange);
}

void FVulkanCommandListContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIEndTransitions, bShowTransitionEvents, TEXT("RHIEndTransitions"));

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	const ERHIPipeline CurrentPipeline = Device->IsRealAsyncComputeContext(this) ? ERHIPipeline::AsyncCompute : ERHIPipeline::Graphics;

	bool bSeenWaitSemaphore = false;
	for (const FRHITransition* Transition : Transitions)
	{
		const FVulkanPipelineBarrier* Data = Transition->GetPrivateData<FVulkanPipelineBarrier>();
		if (Data->Semaphore == nullptr)
		{
			continue;
		}

		// If the source matches our current queue, we don't need a semaphore (avoid unnecessary semaphores when DstPipelines is ERHIPipeline::All)
		if (EnumHasAllFlags(Data->DstPipelines, Data->SrcPipelines) && (Data->SrcPipelines == CurrentPipeline))
		{
			continue;
		}

		if (!bSeenWaitSemaphore)
		{
			if (CommandBufferManager->HasPendingActiveCmdBuffer())
			{
				CommandBufferManager->SubmitActiveCmdBuffer();
				CommandBufferManager->PrepareForNewActiveCommandBuffer();
				CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
			}
			bSeenWaitSemaphore = true;
		}

		CmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, Data->Semaphore);
	}

	TArray<VkBufferMemoryBarrier, TInlineAllocator<8>> RealBufferBarriers;
	TArray<VkImageMemoryBarrier, TInlineAllocator<8>> RealImageBarriers;
	FVulkanPipelineBarrier RealMemoryBarrier;

	for (const FRHITransition* Transition : Transitions)
	{
		const FVulkanPipelineBarrier* Data = Transition->GetPrivateData<FVulkanPipelineBarrier>();
		
		if (Data->SrcStageMask == 0 || Data->DstStageMask == 0)
		{
			// This transition didn't specify any resources.
			check(Data->ImageBarriers.Num() == 0 && Data->BufferBarriers.Num() == 0);
			continue;
		}

#if DO_GUARD_SLOW
		switch (Data->DstPipelines)
		{
		case ERHIPipeline::Graphics:
			checkf(!Device->IsRealAsyncComputeContext(this), TEXT("Attempt to end AsyncCompute -> Graphics transition on the async compute command list."));
			break;
		case ERHIPipeline::AsyncCompute:
			checkf(Device->IsRealAsyncComputeContext(this), TEXT("Attempt to begin Graphics -> AsyncCompute transition on the graphics command list."));
			break;
		default:
			checkNoEntry();
			break;
		}
#endif

		RealMemoryBarrier.MemoryBarrier = Data->MemoryBarrier;
		RealMemoryBarrier.bHasMemoryBarrier = Data->bHasMemoryBarrier;

		check(Data->SrcPipelines != Data->DstPipelines || Data->BufferBarriers.Num() == 0);
		RealBufferBarriers.Reset();
		RealBufferBarriers.Reserve(Data->BufferBarriers.Num());
		for (const VkBufferMemoryBarrier& BufferBarrier : Data->BufferBarriers)
		{
			VkBufferMemoryBarrier& RealBarrier = RealBufferBarriers.AddDefaulted_GetRef();
			RealBarrier = BufferBarrier;
			RealBarrier.srcAccessMask = 0; // Acquire resource on current queue.
		}

		check(Data->ImageBarriers.Num() == Data->ImageBarrierExtras.Num());
		RealImageBarriers.Reset();
		RealImageBarriers.Reserve(Data->ImageBarriers.Num());

		VkPipelineStageFlags RealSrcStageMask = Data->SrcStageMask;
		VkPipelineStageFlags RealDstStageMask = Data->DstStageMask;

		for (int32 ImgBarrierIdx = 0; ImgBarrierIdx < Data->ImageBarriers.Num(); ++ImgBarrierIdx)
		{
			const VkImageMemoryBarrier& ImageBarrier = Data->ImageBarriers[ImgBarrierIdx];
			check(ImageBarrier.image == VK_NULL_HANDLE);

			const FVulkanPipelineBarrier::ImageBarrierExtraData& ExtraData = Data->ImageBarrierExtras[ImgBarrierIdx];
			FVulkanTexture* Texture = ExtraData.BaseTexture;
			check(Texture->GetCpuReadbackBuffer() == nullptr);

			Texture->OnLayoutTransition(*this, ImageBarrier.newLayout);

			if (Texture->Image == VK_NULL_HANDLE)
			{
				// If Texture is a backbuffer, OnLayoutTransition normally updates the image handle. However, if the viewport got invalidated during the
				// OnLayoutTransition call, we may end up with a null handle, which is fine. Just ignore the transition.
				continue;
			}

			FVulkanImageLayout& Layout = LayoutManager.GetOrAddFullLayout(*Texture, VK_IMAGE_LAYOUT_UNDEFINED);

			VkAccessFlags SrcAccessFlags;
			VkImageLayout SrcLayout, DstLayout;

			check(ImageBarrier.newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			DstLayout = ImageBarrier.newLayout;

			if ((ImageBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (!ExtraData.IsAliasingBarrier))
			{
				if (Layout.AreAllSubresourcesSameLayout())
				{
					SrcLayout = Layout.MainLayout;
					SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
					RealSrcStageMask |= GetVkStageFlagsForLayout(SrcLayout);
				}
				else
				{
					// Slow path, adds one transition per subresource.
					check(Data->SrcPipelines == Data->DstPipelines);
					AddSubresourceTransitions(RealImageBarriers, RealSrcStageMask, ImageBarrier, Texture->Image, Layout, DstLayout);
					continue;
				}
			}
			else
			{
				checkSlow(ExtraData.IsAliasingBarrier || Layout.AreSubresourcesSameLayout(ImageBarrier.oldLayout, ImageBarrier.subresourceRange));
				SrcLayout = ImageBarrier.oldLayout;
				SrcAccessFlags = ImageBarrier.srcAccessMask;
			}

			VkImageMemoryBarrier RealBarrier = ImageBarrier;
			RealBarrier.image = Texture->Image; // Use the up to date image handle.
			RealBarrier.srcAccessMask = SrcAccessFlags;
			RealBarrier.oldLayout = SrcLayout;
			RealBarrier.newLayout = DstLayout;

			// Fix up the destination layout if this barrier specifies a sub-aspect of a depth-stencil surface.
			AdjustDepthStencilLayout(RealBarrier, Texture->GetFullAspectMask());

			if (Data->SrcPipelines == Data->DstPipelines)
			{
				if (RealBarrier.oldLayout == RealBarrier.newLayout)
				{
					// It turns out that we don't need a layout transition afterall. We may still need a memory barrier if the
					// previous access was writable.
					RealMemoryBarrier.AddMemoryBarrier(RealBarrier.srcAccessMask, RealBarrier.dstAccessMask, 0, 0);
					continue;
				}
			}
			else
			{
				// Acquire resource on current queue.
				RealBarrier.srcAccessMask = 0;
			}

			RealImageBarriers.Add(RealBarrier);

			Layout.Set(RealBarrier.newLayout, RealBarrier.subresourceRange);
		}

		if ((!RealMemoryBarrier.bHasMemoryBarrier) && (RealBufferBarriers.Num() == 0) && (RealImageBarriers.Num() == 0))
		{
			continue;
		}

		if (EnumHasAnyFlags(Data->DstPipelines, ERHIPipeline::AsyncCompute))
		{
			RealDstStageMask = RealDstStageMask & Queue->GetSupportedStageBits();
		}

		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), RealSrcStageMask, RealDstStageMask, 0, RealMemoryBarrier.bHasMemoryBarrier ? 1 : 0, &RealMemoryBarrier.MemoryBarrier, RealBufferBarriers.Num(), RealBufferBarriers.GetData(), RealImageBarriers.Num(), RealImageBarriers.GetData());
	}
}


void FVulkanPipelineBarrier::AddMemoryBarrier(VkAccessFlags InSrcAccessFlags, VkAccessFlags InDstAccessFlags, VkPipelineStageFlags InSrcStageMask, VkPipelineStageFlags InDstStageMask)
{
	const VkAccessFlags ReadMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;

	// We only need a memory barrier if the previous commands wrote to the buffer. In case of a transition from read, an execution barrier is enough.
	const bool SrcAccessIsRead = ((InSrcAccessFlags & (~ReadMask)) == 0);
	if (!SrcAccessIsRead)
	{
		MemoryBarrier.srcAccessMask |= InSrcAccessFlags;
		MemoryBarrier.dstAccessMask |= InDstAccessFlags;
	}

	SrcStageMask |= InSrcStageMask;
	DstStageMask |= InDstStageMask;
	bHasMemoryBarrier = true;
}


//
// Methods used when the RHI itself needs to perform a layout transition. The public API functions do not call these,
// they fill in the fields of FVulkanPipelineBarrier using their own logic, based on the ERHIAccess flags.
//
void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresourceRange)
{
	SrcStageMask |= GetVkStageFlagsForLayout(SrcLayout);
	DstStageMask |= GetVkStageFlagsForLayout(DstLayout);

	VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
	VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

	VkImageMemoryBarrier& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
	SetupImageBarrier(ImgBarrier, Image, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const FVulkanImageLayout& SrcLayout, VkImageLayout DstLayout)
{
	if (SrcLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, SrcLayout.MainLayout, DstLayout, MakeSubresourceRange(AspectMask));
		return;
	}

	DstStageMask |= GetVkStageFlagsForLayout(DstLayout);
	const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

	VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(AspectMask, 0, 1, 0, 1);
	for (; SubresourceRange.baseArrayLayer < SrcLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
	{
		for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < SrcLayout.NumMips; ++SubresourceRange.baseMipLevel)
		{
			const VkImageLayout SubresourceLayout = SrcLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel);
			if (SubresourceLayout != DstLayout)
			{
				SrcStageMask |= GetVkStageFlagsForLayout(SubresourceLayout);
				const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SubresourceLayout);

				VkImageMemoryBarrier& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
				SetupImageBarrier(ImgBarrier, Image, SrcAccessFlags, DstAccessFlags, SubresourceLayout, DstLayout, SubresourceRange);
			}
		}
	}
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout SrcLayout, const FVulkanImageLayout& DstLayout)
{
	if (DstLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, SrcLayout, DstLayout.MainLayout, MakeSubresourceRange(AspectMask));
		return;
	}

	SrcStageMask |= GetVkStageFlagsForLayout(SrcLayout);
	const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);

	VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(AspectMask, 0, 1, 0, 1);
	for (; SubresourceRange.baseArrayLayer < DstLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
	{
		for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < DstLayout.NumMips; ++SubresourceRange.baseMipLevel)
		{
			const VkImageLayout SubresourceLayout = DstLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel);
			if (SubresourceLayout != SrcLayout)
			{
				DstStageMask |= GetVkStageFlagsForLayout(SubresourceLayout);
				const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(SubresourceLayout);

				VkImageMemoryBarrier& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
				SetupImageBarrier(ImgBarrier, Image, SrcAccessFlags, DstAccessFlags, SrcLayout, SubresourceLayout, SubresourceRange);
			}
		}
	}
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const FVulkanImageLayout& SrcLayout, const FVulkanImageLayout& DstLayout)
{
	if (SrcLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, AspectMask, SrcLayout.MainLayout, DstLayout);
	}
	else if (DstLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, AspectMask, SrcLayout, DstLayout.MainLayout);
	}
	else
	{
		checkf(SrcLayout.NumLayers == DstLayout.NumLayers, TEXT("Source (%d) and Destination (%d) layer count mismatch!"), SrcLayout.NumLayers, DstLayout.NumLayers);
		checkf(SrcLayout.NumMips == DstLayout.NumMips, TEXT("Source (%d) and Destination (%d) mip count mismatch!"), SrcLayout.NumMips, DstLayout.NumMips);

		VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(AspectMask, 0, 1, 0, 1);
		for (; SubresourceRange.baseArrayLayer < DstLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
		{
			for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < DstLayout.NumMips; ++SubresourceRange.baseMipLevel)
			{
				const VkImageLayout SrcSubresourceLayout = SrcLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel);
				const VkImageLayout DstSubresourceLayout = DstLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel);
				if (SrcSubresourceLayout != DstSubresourceLayout)
				{
					SrcStageMask |= GetVkStageFlagsForLayout(SrcSubresourceLayout);
					const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcSubresourceLayout);

					DstStageMask |= GetVkStageFlagsForLayout(DstSubresourceLayout);
					const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstSubresourceLayout);

					VkImageMemoryBarrier& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
					SetupImageBarrier(ImgBarrier, Image, SrcAccessFlags, DstAccessFlags, SrcSubresourceLayout, DstSubresourceLayout, SubresourceRange);
				}
			}
		}
	}
}

void FVulkanPipelineBarrier::AddImageAccessTransition(const FVulkanTexture& Surface, ERHIAccess SrcAccess, ERHIAccess DstAccess, const VkImageSubresourceRange& SubresourceRange, VkImageLayout& InOutLayout)
{
	// This function should only be used for known states.
	check(DstAccess != ERHIAccess::Unknown);
	const bool bIsDepthStencil = Surface.IsDepthOrStencilAspect();
	const bool bSupportsReadOnlyOptimal = Surface.SupportsSampling();

	VkPipelineStageFlags ImgSrcStage, ImgDstStage;
	VkAccessFlags SrcAccessFlags, DstAccessFlags;
	VkImageLayout SrcLayout, DstLayout;
	GetVkStageAndAccessFlags(SrcAccess, FRHITransitionInfo::EType::Texture, 0, bIsDepthStencil, bSupportsReadOnlyOptimal, ImgSrcStage, SrcAccessFlags, SrcLayout, true);
	GetVkStageAndAccessFlags(DstAccess, FRHITransitionInfo::EType::Texture, 0, bIsDepthStencil, bSupportsReadOnlyOptimal, ImgDstStage, DstAccessFlags, DstLayout, false);

	// If not compute, remove vertex pipeline bits as only compute updates vertex buffers
	if (!(ImgSrcStage & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))
	{
		ImgDstStage &= ~(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
	}

	SrcStageMask |= ImgSrcStage;
	DstStageMask |= ImgDstStage;

	if (SrcLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		SrcLayout = InOutLayout;
		SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
	}
	else
	{
		ensure(SrcLayout == InOutLayout);
	}

	if (DstLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		DstLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkImageMemoryBarrier& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
	SetupImageBarrier(ImgBarrier, Surface.Image, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);

	InOutLayout = DstLayout;
}

void FVulkanPipelineBarrier::Execute(VkCommandBuffer CmdBuffer)
{
	if (bHasMemoryBarrier || ImageBarriers.Num() != 0)
	{
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer, SrcStageMask, DstStageMask, 0, bHasMemoryBarrier ? 1 : 0, &MemoryBarrier, 0, nullptr, ImageBarriers.Num(), ImageBarriers.GetData());
	}
}

VkImageSubresourceRange FVulkanPipelineBarrier::MakeSubresourceRange(VkImageAspectFlags AspectMask, uint32 FirstMip, uint32 NumMips, uint32 FirstLayer, uint32 NumLayers)
{
	VkImageSubresourceRange Range;
	Range.aspectMask = AspectMask;
	Range.baseMipLevel = FirstMip;
	Range.levelCount = NumMips;
	Range.baseArrayLayer = FirstLayer;
	Range.layerCount = NumLayers;
	return Range;
}

//
// Used when we need to change the layout of a single image. Some plug-ins call this function from outside the RHI (Steam VR, at the time of writing this).
//
void VulkanSetImageLayout(VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange)
{
	FVulkanPipelineBarrier Barrier;
	Barrier.AddImageLayoutTransition(Image, OldLayout, NewLayout, SubresourceRange);
	Barrier.Execute(CmdBuffer);
}

bool FVulkanImageLayout::AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange) const
{
	if (SubresLayouts.Num() == 0)
	{
		return MainLayout == Layout;
	}

	const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
	const uint32 LastLayer = FirstLayer + GetSubresRangeLayerCount(SubresourceRange);

	const uint32 FirstMip = SubresourceRange.baseMipLevel;
	const uint32 LastMip = FirstMip + GetSubresRangeMipCount(SubresourceRange);

	for (uint32 LayerIdx = FirstLayer; LayerIdx < LastLayer; ++LayerIdx)
	{
		for (uint32 MipIdx = FirstMip; MipIdx < LastMip; ++MipIdx)
		{
			if (SubresLayouts[LayerIdx * NumMips + MipIdx] != Layout)
			{
				return false;
			}
		}
	}

	return true;
}

void FVulkanImageLayout::CollapseSubresLayoutsIfSame()
{
	if (SubresLayouts.Num() == 0)
	{
		return;
	}

	VkImageLayout Layout = SubresLayouts[0];
	for (uint32 i = 1; i < NumLayers * NumMips; ++i)
	{
		if (SubresLayouts[i] != Layout)
		{
			return;
		}
	}

	MainLayout = Layout;
	SubresLayouts.Reset();
}

void FVulkanImageLayout::Set(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange)
{
	const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
	const uint32 LayerCount = GetSubresRangeLayerCount(SubresourceRange);

	const uint32 FirstMip = SubresourceRange.baseMipLevel;
	const uint32 MipCount = GetSubresRangeMipCount(SubresourceRange);

	if (FirstLayer == 0 && LayerCount == NumLayers && FirstMip == 0 && MipCount == NumMips)
	{
		// We're setting the entire resource to the same layout.
		MainLayout = Layout;
		SubresLayouts.Reset();
		return;
	}

	if (SubresLayouts.Num() == 0)
	{
		uint32 SubresLayoutCount = (NumLayers + FirstLayer) * NumMips;
		SubresLayouts.SetNum(SubresLayoutCount);
		for (uint32 i = 0; i < SubresLayoutCount; ++i)
		{
			SubresLayouts[i] = MainLayout;
		}
	}

	for (uint32 Layer = FirstLayer; Layer < FirstLayer + LayerCount; ++Layer)
	{
		for (uint32 Mip = FirstMip; Mip < FirstMip + MipCount; ++Mip)
		{
			SubresLayouts[Layer * NumMips + Mip] = Layout;
		}
	}

	// It's possible we've just set all the subresources to the same layout. If that's the case, get rid of the
	// subresource info and set the main layout appropriatedly.
	CollapseSubresLayoutsIfSame();
}

void FVulkanLayoutManager::Destroy(FVulkanDevice& InDevice, FVulkanLayoutManager* Immediate)
{
	check(!GIsRHIInitialized);

	if (Immediate)
	{
		Immediate->RenderPasses.Append(RenderPasses);
		Immediate->Framebuffers.Append(Framebuffers);
	}
	else
	{
		for (auto& Pair : RenderPasses)
		{
			delete Pair.Value;
		}

		for (auto& Pair : Framebuffers)
		{
			FFramebufferList* List = Pair.Value;
			for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
			{
				List->Framebuffer[Index]->Destroy(InDevice);
				delete List->Framebuffer[Index];
			}
			delete List;
		}
	}

	RenderPasses.Reset();
	Framebuffers.Reset();
}

FVulkanFramebuffer* FVulkanLayoutManager::GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass)
{
	uint32 RTLayoutHash = RTLayout.GetRenderPassCompatibleHash();

	uint64 MipsAndSlicesValues[MaxSimultaneousRenderTargets];
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		MipsAndSlicesValues[Index] = ((uint64)RenderTargetsInfo.ColorRenderTarget[Index].ArraySliceIndex << (uint64)32) | (uint64)RenderTargetsInfo.ColorRenderTarget[Index].MipIndex;
	}
	RTLayoutHash = FCrc::MemCrc32(MipsAndSlicesValues, sizeof(MipsAndSlicesValues), RTLayoutHash);

	FFramebufferList** FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
	FFramebufferList* FramebufferList = nullptr;
	if (FoundFramebufferList)
	{
		FramebufferList = *FoundFramebufferList;

		for (int32 Index = 0; Index < FramebufferList->Framebuffer.Num(); ++Index)
		{
			const VkRect2D RenderArea = FramebufferList->Framebuffer[Index]->GetRenderArea();

			if (FramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo) &&
				((RTLayout.GetExtent2D().width == RenderArea.extent.width) && (RTLayout.GetExtent2D().height == RenderArea.extent.height) &&
				(RTLayout.GetOffset2D().x == RenderArea.offset.x) && (RTLayout.GetOffset2D().y == RenderArea.offset.y)))
			{
				return FramebufferList->Framebuffer[Index];
			}
		}
	}
	else
	{
		FramebufferList = new FFramebufferList;
		Framebuffers.Add(RTLayoutHash, FramebufferList);
	}

	FVulkanFramebuffer* Framebuffer = new FVulkanFramebuffer(InDevice, RenderTargetsInfo, RTLayout, *RenderPass);
	FramebufferList->Framebuffer.Add(Framebuffer);
	return Framebuffer;
}

void FVulkanLayoutManager::ValidateRenderPassColorEntry(const FRHIRenderPassInfo::FColorEntry& ColorEntry, bool bResolveTarget, FVulkanPipelineBarrier& Barrier)
{
	FRHITexture* Texture = bResolveTarget ? ColorEntry.ResolveTarget : ColorEntry.RenderTarget;
	CA_ASSUME(Texture);
	FVulkanTexture& Surface = *FVulkanTexture::Cast(Texture);
	check(Surface.Image != VK_NULL_HANDLE);

	// Check that the image is in the correct layout for rendering.
	const VkImageLayout ExpectedVkLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	FVulkanImageLayout& LayoutInfo = GetOrAddFullLayout(Surface, VK_IMAGE_LAYOUT_UNDEFINED);
	const VkImageLayout CurrentVkLayout = LayoutInfo.GetSubresLayout(ColorEntry.ArraySlice, ColorEntry.MipIndex);
	if (ensureMsgf(CurrentVkLayout == ExpectedVkLayout, TEXT("%s target is in layout %s, expected %s. Please add a transition before starting the render pass."), bResolveTarget ? TEXT("Resolve") : TEXT("Color"), VK_TYPE_TO_STRING(VkImageLayout, CurrentVkLayout), VK_TYPE_TO_STRING(VkImageLayout, ExpectedVkLayout)))
	{
		return;
	}

	// If all the subresources are in the same layout, add a transition for the entire image.
	if (LayoutInfo.AreAllSubresourcesSameLayout())
	{
		Barrier.AddImageLayoutTransition(Surface.Image, CurrentVkLayout, ExpectedVkLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
		LayoutInfo.MainLayout = ExpectedVkLayout;
		return;
	}

	// Transition only the mip and layer we're rendering to.
	VkImageSubresourceRange SubresRange = FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, ColorEntry.MipIndex, 1, ColorEntry.ArraySlice, 1);
	Barrier.AddImageLayoutTransition(Surface.Image, CurrentVkLayout, ExpectedVkLayout, SubresRange);
	LayoutInfo.Set(ExpectedVkLayout, SubresRange);
}

void FVulkanLayoutManager::BeginRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHIRenderPassInfo& RPInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer)
{
	check(!CurrentRenderPass);
	// (NumRT + 1 [Depth] ) * 2 [surface + resolve]
	VkClearValue ClearValues[(MaxSimultaneousRenderTargets + 1) * 2];
	uint32 ClearValueIndex = 0;
	bool bNeedsClearValues = RenderPass->GetNumUsedClearValues() > 0;
	FMemory::Memzero(ClearValues);

	int32 NumColorTargets = RPInfo.GetNumColorRenderTargets();
	int32 Index = 0;

	FVulkanPipelineBarrier Barrier;

	for (Index = 0; Index < NumColorTargets; ++Index)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorEntry = RPInfo.ColorRenderTargets[Index];

		FRHITexture* ColorTexture = ColorEntry.RenderTarget;
		CA_ASSUME(ColorTexture);
		FVulkanTexture& ColorSurface = *FVulkanTexture::Cast(ColorTexture);
		const bool bPassPerformsResolve = ColorSurface.GetNumSamples() > 1 && ColorEntry.ResolveTarget;

		ValidateRenderPassColorEntry(ColorEntry, false, Barrier);
		if (bPassPerformsResolve)
		{
			check(ColorEntry.ResolveTarget != ColorEntry.RenderTarget);
			ValidateRenderPassColorEntry(ColorEntry, true, Barrier);
		}

		if (GetLoadAction(ColorEntry.Action) == ERenderTargetLoadAction::ELoad)
		{
			// Insert a barrier if we're loading from any color targets, to make sure the passes aren't reordered and we end up running before
			// the pass we're supposed to read from.
			const VkAccessFlags AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			const VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Barrier.AddMemoryBarrier(AccessMask, AccessMask, StageMask, StageMask);
		}

		if (bNeedsClearValues)
		{
			const FLinearColor& ClearColor = ColorTexture->HasClearValue() ? ColorTexture->GetClearColor() : FLinearColor::Black;
			ClearValues[ClearValueIndex].color.float32[0] = ClearColor.R;
			ClearValues[ClearValueIndex].color.float32[1] = ClearColor.G;
			ClearValues[ClearValueIndex].color.float32[2] = ClearColor.B;
			ClearValues[ClearValueIndex].color.float32[3] = ClearColor.A;
			++ClearValueIndex;
			if (bPassPerformsResolve)
			{
				++ClearValueIndex;
			}
		}
	}

	FRHITexture* DSTexture = RPInfo.DepthStencilRenderTarget.DepthStencilTarget;
	if (DSTexture)
	{
		FExclusiveDepthStencil RequestedDSAccess = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
		if (RequestedDSAccess.IsDepthRead() || RequestedDSAccess.IsStencilRead())
		{
			// If the depth-stencil state doesn't change between passes, the high level code won't perform any transitions.
			// Make sure we have a barrier in case we're loading depth or stencil, to prevent rearranging passes.
			const VkAccessFlags AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			const VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			Barrier.AddMemoryBarrier(AccessMask, AccessMask, StageMask, StageMask);
		}

		if (DSTexture->HasClearValue() && bNeedsClearValues)
		{
			float Depth = 0;
			uint32 Stencil = 0;
			DSTexture->GetDepthStencilClearValue(Depth, Stencil);
			ClearValues[ClearValueIndex].depthStencil.depth = Depth;
			ClearValues[ClearValueIndex].depthStencil.stencil = Stencil;
			++ClearValueIndex;
		}
	}

	FRHITexture* ShadingRateTexture = RPInfo.ShadingRateTexture;
	if (ShadingRateTexture && ValidateShadingRateDataType())
	{
		FVulkanTexture& Surface = *FVulkanTexture::Cast(ShadingRateTexture);
		VkImageLayout& DSLayout = FindOrAddLayoutRW(Surface, VK_IMAGE_LAYOUT_UNDEFINED);
		
		VkImageLayout ExpectedLayout = VK_IMAGE_LAYOUT_UNDEFINED;

#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			ExpectedLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}
#endif

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			ExpectedLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}
#endif

		// transition shading rate textures to the fragment density map layout for rendering
		if (DSLayout != ExpectedLayout)
		{
			Barrier.AddImageLayoutTransition(Surface.Image, DSLayout, ExpectedLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
			DSLayout = ExpectedLayout;
		}
	}

	ensure(ClearValueIndex <= RenderPass->GetNumUsedClearValues());

	Barrier.Execute(CmdBuffer->GetHandle());

	CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass, Framebuffer, ClearValues);

	{
		const VkExtent3D& Extents = RTLayout.GetExtent3D();
		Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
	}

	CurrentFramebuffer = Framebuffer;
	CurrentRenderPass = RenderPass;
}

void FVulkanLayoutManager::EndRenderPass(FVulkanCmdBuffer* CmdBuffer)
{
	check(CurrentRenderPass);
	CmdBuffer->EndRenderPass();

	CurrentRenderPass = nullptr;

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer->GetHandle(), 1);
}

void FVulkanLayoutManager::NotifyDeletedImage(VkImage Image)
{
	Layouts.Remove(Image);
}

void FVulkanLayoutManager::NotifyDeletedRenderTarget(FVulkanDevice& InDevice, VkImage Image)
{
	Layouts.Remove(Image);
	for (auto It = Framebuffers.CreateIterator(); It; ++It)
	{
		FFramebufferList* List = It->Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			FVulkanFramebuffer* Framebuffer = List->Framebuffer[Index];
			if (Framebuffer->ContainsRenderTarget(Image))
			{
				List->Framebuffer.RemoveAtSwap(Index, 1, false);
				Framebuffer->Destroy(InDevice);

				if (Framebuffer == CurrentFramebuffer)
				{
					CurrentFramebuffer = nullptr;
				}

				delete Framebuffer;
			}
		}

		if (List->Framebuffer.Num() == 0)
		{
			delete List;
			It.RemoveCurrent();
		}
	}
}
