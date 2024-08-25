// Copyright Epic Games, Inc. All Rights Reserved..

#include "VulkanRHIPrivate.h"
#include "VulkanBarriers.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"



// All shader stages supported by VK device - VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, FRAGMENT etc
extern uint32 GVulkanDevicePipelineStageBits;


int32 GVulkanUseMemoryBarrierOpt = 1;
static FAutoConsoleVariableRef CVarVulkanUseMemoryBarrierOpt(
	TEXT("r.Vulkan.UseMemoryBarrierOpt"),
	GVulkanUseMemoryBarrierOpt,
	TEXT("Simplify buffer barriers and image barriers without layout transitions to a memory barrier.\n")
	TEXT(" 0: Do not collapse to a single memory barrier, useful for tracking single resource transitions in external tools\n")
	TEXT(" 1: Collapse to a memory barrier when appropriate (default)"),
	ECVF_Default
);

int32 GVulkanAutoCorrectExpectedLayouts = 1;
static FAutoConsoleVariableRef CVarVulkanAutoCorrectExpectedLayouts(
	TEXT("r.Vulkan.AutoCorrectExpectedLayouts"),
	GVulkanAutoCorrectExpectedLayouts,
	TEXT("Will use layout tracking to correct mismatched layouts when setting expected layouts.  \n")
	TEXT("This is unsafe for multi-threaded command buffer generations\n")
	TEXT(" 0: Do not correct layouts.\n")
	TEXT(" 1: Correct the layouts using layout tracking. (default)"),
	ECVF_Default
);

int32 GVulkanAutoCorrectUnknownLayouts = 1;
static FAutoConsoleVariableRef CVarVulkanAutoCorrectUnknownLayouts(
	TEXT("r.Vulkan.AutoCorrectUnknownLayouts"),
	GVulkanAutoCorrectUnknownLayouts,
	TEXT("Will use layout tracking to correct unknown layouts.  \n")
	TEXT("This is unsafe for multi-threaded command buffer generations\n")
	TEXT(" 0: Do not correct layouts.\n")
	TEXT(" 1: Correct the layouts using layout tracking. (default)"),
	ECVF_Default
);

//
// The following two functions are used when the RHI needs to do image layout transitions internally.
// They are not used for the transitions requested through the public API (RHICreate/Begin/EndTransition)
// unless the initial state in ERHIAccess::Unknown, in which case the tracking code kicks in.
//
static VkAccessFlags GetVkAccessMaskForLayout(const VkImageLayout Layout)
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
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
			Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = 0;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
			Flags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			Flags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			break;

		case VK_IMAGE_LAYOUT_GENERAL:
			// todo-jn: could be used for R64 in read layout
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = 0;
			break;

		case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
			// todo-jn: sync2 currently only used by depth/stencil targets
			Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
			// todo-jn: sync2 currently only used by depth/stencil targets
			Flags = VK_ACCESS_SHADER_READ_BIT;
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
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			break;
			
		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;

		case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
			// todo-jn: sync2 currently only used by depth/stencil targets
			Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
			// todo-jn: sync2 currently only used by depth/stencil targets
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
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
		bIsDepthStencil ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL : 
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

		case ERHIAccess::BVHRead:
			// vkrt todo: Finer grain stage flags would be ideal here.
			StageFlags = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			AccessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			return;

		case ERHIAccess::BVHWrite:
			// vkrt todo: Finer grain stage flags would be ideal here.
			StageFlags = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			AccessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			return;
	}

	// If DSVWrite is set, we ignore everything else because it decides the layout.
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVWrite))
	{
		check(bIsDepthStencil);
		StageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		Layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
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
		Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVRead;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVGraphics))
	{
		StageFlags |= (GVulkanDevicePipelineStageBits & ~VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		if ((UsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
		{
			AccessFlags |= VK_ACCESS_UNIFORM_READ_BIT;
		}
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

	// ResolveSrc is used when doing a resolve via RHICopyToResolveTarget. For us, it's the same as CopySrc.
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::CopySrc | ERHIAccess::ResolveSrc))
	{
		// If this is requested for a texture, behavior will depend on if we're combined with other flags
		if (ResourceType == FRHITransitionInfo::EType::Texture)
		{
			// If no other RHIAccess is mixed in with our CopySrc, then use proper TRANSFER_SRC layout
			if (Layout == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
				AccessFlags = VK_ACCESS_TRANSFER_READ_BIT;
			}
			else
			{
				// If anything else is mixed in with the CopySrc, then go to the "catch all" GENERAL layout
				Layout = VK_IMAGE_LAYOUT_GENERAL;
				StageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
				AccessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
			}
		}
		else
		{
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			AccessFlags = VK_ACCESS_TRANSFER_READ_BIT;
		}

		ProcessedRHIFlags |= (uint32)(ERHIAccess::CopySrc | ERHIAccess::ResolveSrc);
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::ShadingRateSource) && ValidateShadingRateDataType())
	{
		checkf(ResourceType == FRHITransitionInfo::EType::Texture, TEXT("A non-texture resource was tagged as a shading rate source; only textures (Texture2D and Texture2DArray) can be used for this purpose."));
		
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			AccessFlags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			Layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}

		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
			AccessFlags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
			Layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}

		ProcessedRHIFlags |= (uint32)ERHIAccess::ShadingRateSource;
	}

	uint32 RemainingFlags = (uint32)RHIAccess & (~ProcessedRHIFlags);
	ensureMsgf(RemainingFlags == 0, TEXT("Some access flags were not processed. RHIAccess=%x, ProcessedRHIFlags=%x, RemainingFlags=%x"), RHIAccess, ProcessedRHIFlags, RemainingFlags);
}


static VkImageAspectFlags GetDepthStencilAspectMask(uint32 PlaneSlice)
{
	VkImageAspectFlags AspectFlags = 0;

	if (PlaneSlice == FRHISubresourceRange::kAllSubresources)
	{
		AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	if (PlaneSlice == FRHISubresourceRange::kDepthPlaneSlice)
	{
		AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	if (PlaneSlice == FRHISubresourceRange::kStencilPlaneSlice)
	{
		AspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	return AspectFlags;
}


// Returns the VK_KHR_synchronization2 layout corresponding to an access type
static VkImageLayout GetDepthOrStencilLayout(ERHIAccess Access)
{
	VkImageLayout Layout;
	if (Access == ERHIAccess::Unknown)
	{
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	else if (Access == ERHIAccess::Discard)
	{
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	else if (EnumHasAnyFlags(Access, ERHIAccess::CopySrc))
	{
		Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if (EnumHasAnyFlags(Access, ERHIAccess::CopyDest))
	{
		Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if (EnumHasAnyFlags(Access, ERHIAccess::DSVWrite))
	{
		Layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	}
	else
	{
		Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}
	return Layout;
}

static void GetDepthOrStencilStageAndAccessFlags(ERHIAccess Access, VkPipelineStageFlags& StageFlags, VkAccessFlags& AccessFlags)
{
	if (Access == ERHIAccess::Unknown || Access == ERHIAccess::Discard)
	{
		StageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		AccessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		return;
	}

	StageFlags = 0;
	AccessFlags = 0;
	uint32 ProcessedRHIFlags = 0;

	if (EnumHasAnyFlags(Access, ERHIAccess::DSVWrite))
	{
		StageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVWrite;
	}
	
	if (EnumHasAnyFlags(Access, ERHIAccess::DSVRead))
	{
		StageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		AccessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::DSVRead;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::SRVGraphics))
	{
		StageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVGraphics;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::UAVGraphics))
	{
		StageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVGraphics;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::SRVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::SRVCompute;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::UAVCompute))
	{
		StageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		AccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::UAVCompute;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::CopySrc))
	{
		StageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		AccessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::CopySrc;
	}

	if (EnumHasAnyFlags(Access, ERHIAccess::CopyDest))
	{
		StageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		AccessFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
		ProcessedRHIFlags |= (uint32)ERHIAccess::CopyDest;
	}

	const uint32 RemainingFlags = (uint32)Access & (~ProcessedRHIFlags);
	ensureMsgf(RemainingFlags == 0, TEXT("Some access flags were not processed. Access=%x, ProcessedRHIFlags=%x, RemainingFlags=%x"), Access, ProcessedRHIFlags, RemainingFlags);
}


// Helper To apply to each bit in an aspect
template <typename TFunction>
static void ForEachAspect(VkImageAspectFlags AspectFlags, TFunction Function)
{
	// Keep it simple for now, can iterate on more bits when needed
	for (uint32 SingleBit = 1; SingleBit <= VK_IMAGE_ASPECT_STENCIL_BIT; SingleBit <<= 1)
	{
		if ((AspectFlags & SingleBit) != 0)
		{
			Function((VkImageAspectFlagBits)SingleBit);
		}
	}
}

//
// Helpers for filling in the fields of a VkImageMemoryBarrier structure.
//
static void SetupImageBarrier(VkImageMemoryBarrier2& ImgBarrier, VkImage Image, VkPipelineStageFlags SrcStageFlags, VkPipelineStageFlags DstStageFlags, 
	VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresRange)
{
	ImgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ImgBarrier.pNext = nullptr;
	ImgBarrier.srcStageMask = SrcStageFlags;
	ImgBarrier.dstStageMask = DstStageFlags;
	ImgBarrier.srcAccessMask = SrcAccessFlags;
	ImgBarrier.dstAccessMask = DstAccessFlags;
	ImgBarrier.oldLayout = SrcLayout;
	ImgBarrier.newLayout = DstLayout;
	ImgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImgBarrier.image = Image;
	ImgBarrier.subresourceRange = SubresRange;
}

static void SetupImageBarrierEntireRes(VkImageMemoryBarrier2& ImgBarrier, VkImage Image, VkPipelineStageFlags SrcStageFlags, VkPipelineStageFlags DstStageFlags, 
	VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkImageLayout SrcLayout, VkImageLayout DstLayout, VkImageAspectFlags AspectMask)
{
	VkImageSubresourceRange SubresRange;
	SubresRange.aspectMask = AspectMask;
	SubresRange.baseMipLevel = 0;
	SubresRange.levelCount = VK_REMAINING_MIP_LEVELS;
	SubresRange.baseArrayLayer = 0;
	SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	SetupImageBarrier(ImgBarrier, Image, SrcStageFlags, DstStageFlags, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresRange);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(RHICreateTransition);

	const ERHIPipeline SrcPipelines = CreateInfo.SrcPipelines;
	const ERHIPipeline DstPipelines = CreateInfo.DstPipelines;

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
			FVulkanTexture* Texture = ResourceCast(Info.Texture);
			if (Texture->GetCpuReadbackBuffer() == nullptr)
			{
				++NumTextures;
			}
			continue;
		}

		if (Info.Type == FRHITransitionInfo::EType::UAV)
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(Info.UAV);
			if (UAV->IsTexture())
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
			Texture = ResourceCast(Info.Texture);
			if (Texture->GetCpuReadbackBuffer())
			{
				Texture = nullptr;
			}
			break;
		}

		case FRHITransitionInfo::EType::Buffer:
		{
			Buffer = ResourceCast(Info.Buffer);
			UsageFlags = Buffer->GetBufferUsageFlags();
			break;
		}

		case FRHITransitionInfo::EType::UAV:
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(Info.UAV);
			if (UAV->IsTexture())
			{
				Texture = ResourceCast(UAV->GetTexture());
				UnderlyingType = FRHITransitionInfo::EType::Texture;
			}
			else
			{
				Buffer = ResourceCast(UAV->GetBuffer());
				UnderlyingType = FRHITransitionInfo::EType::Buffer;
				UsageFlags = Buffer->GetBufferUsageFlags();
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

		if (bIsDepthStencil)
		{
			// if we use separate transitions, then just feed them in as they are
			SrcLayout = GetDepthOrStencilLayout(Info.AccessBefore);
			DstLayout = GetDepthOrStencilLayout(Info.AccessAfter);
			GetDepthOrStencilStageAndAccessFlags(Info.AccessBefore, SrcStageMask, SrcAccessFlags);
			GetDepthOrStencilStageAndAccessFlags(Info.AccessAfter, DstStageMask, DstAccessFlags);
		}
		else
		{
			
			const bool bSupportsReadOnlyOptimal = (Texture == nullptr) || Texture->SupportsSampling();

			GetVkStageAndAccessFlags(Info.AccessBefore, UnderlyingType, UsageFlags, bIsDepthStencil, bSupportsReadOnlyOptimal, SrcStageMask, SrcAccessFlags, SrcLayout, true);
			GetVkStageAndAccessFlags(Info.AccessAfter, UnderlyingType, UsageFlags, bIsDepthStencil, bSupportsReadOnlyOptimal, DstStageMask, DstAccessFlags, DstLayout, false);

			// If not compute, remove vertex pipeline bits as only compute updates vertex buffers
			if (!(SrcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))
			{
				DstStageMask &= ~(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
			}
		}

		// If we're not transitioning across pipes and we don't need to perform layout transitions, we can express memory dependencies through a global memory barrier.
		if ((SrcPipelines == DstPipelines) && (Texture == nullptr || (SrcLayout == DstLayout)) && GVulkanUseMemoryBarrierOpt)
		{
			Data->AddMemoryBarrier(SrcAccessFlags, DstAccessFlags, SrcStageMask, DstStageMask);
			continue;
		}

		if (Buffer != nullptr)
		{
			// We only add buffer transitions for cross-pipe transfers.
			checkSlow( (SrcPipelines != DstPipelines) && (Texture == nullptr) );
			VkBufferMemoryBarrier2& BufferBarrier = Data->BufferBarriers.AddDefaulted_GetRef();
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			BufferBarrier.pNext = nullptr;
			BufferBarrier.srcStageMask = SrcStageMask;
			BufferBarrier.dstStageMask = DstStageMask;
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

		const VkImageAspectFlags AspectFlags = bIsDepthStencil ? GetDepthStencilAspectMask(Info.PlaneSlice) : Texture->GetFullAspectMask();
		VkImageSubresourceRange SubresRange;
		SetupSubresourceRange(SubresRange, Info, AspectFlags);

		// For some textures, e.g. FVulkanBackBuffer, the image handle may not be set yet, or may be stale, so there's no point storing it here.
		// We'll set the image to NULL in the barrier info, and RHIEndTransitions will fetch the up to date pointer from the texture, after
		// OnLayoutTransition is called.
		VkImageMemoryBarrier2& ImgBarrier = Data->ImageBarriers.AddDefaulted_GetRef();
		SetupImageBarrier(ImgBarrier, VK_NULL_HANDLE, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresRange);
		ImgBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
		ImgBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;

		FVulkanPipelineBarrier::ImageBarrierExtraData& ExtraData = Data->ImageBarrierExtras.AddDefaulted_GetRef();
		ExtraData.BaseTexture = Texture;
		ExtraData.IsAliasingBarrier = (Info.AccessBefore == ERHIAccess::Discard);
	}
}

void FVulkanDynamicRHI::RHIReleaseTransition(FRHITransition* Transition)
{
	Transition->GetPrivateData<FVulkanPipelineBarrier>()->~FVulkanPipelineBarrier();
}

static void AddSubresourceTransitions(TArray<VkImageMemoryBarrier>& Barriers, VkPipelineStageFlags& SrcStageMask, const VkImageMemoryBarrier TemplateBarrier, VkImage ImageHandle, FVulkanImageLayout& CurrentLayout, VkImageLayout DstLayout)
{
	const bool bIsDepthStencil = VKHasAnyFlags(TemplateBarrier.subresourceRange.aspectMask, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

	const uint32 FirstLayer = TemplateBarrier.subresourceRange.baseArrayLayer;
	const uint32 LastLayer = FirstLayer + CurrentLayout.GetSubresRangeLayerCount(TemplateBarrier.subresourceRange);

	const uint32 FirstMip = TemplateBarrier.subresourceRange.baseMipLevel;
	const uint32 LastMip = FirstMip + CurrentLayout.GetSubresRangeMipCount(TemplateBarrier.subresourceRange);

	for (uint32 LayerIdx = FirstLayer; LayerIdx < LastLayer; ++LayerIdx)
	{
		VkImageMemoryBarrier* PrevMipBarrier = nullptr;

		for (uint32 MipIdx = FirstMip; MipIdx < LastMip; ++MipIdx)
		{
			VkImageLayout SrcLayout = CurrentLayout.GetSubresLayout(LayerIdx, MipIdx, 0);
			if (bIsDepthStencil)
			{
				const VkImageLayout OtherLayout = (CurrentLayout.NumPlanes == 1) ? SrcLayout : CurrentLayout.GetSubresLayout(LayerIdx, MipIdx, 1);
				SrcLayout = GetMergedDepthStencilLayout(SrcLayout, OtherLayout);
			}

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
				Barrier.newLayout = bIsDepthStencil ? GetMergedDepthStencilLayout(DstLayout, DstLayout) : DstLayout;
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

static void DowngradeBarrier(VkMemoryBarrier& OutBarrier, const VkMemoryBarrier2& InBarrier)
{
	OutBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	OutBarrier.pNext = InBarrier.pNext;
	OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
	OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
}

static void DowngradeBarrier(VkBufferMemoryBarrier& OutBarrier, const VkBufferMemoryBarrier2& InBarrier)
{
	OutBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	OutBarrier.pNext = InBarrier.pNext;
	OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
	OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
	OutBarrier.srcQueueFamilyIndex = InBarrier.srcQueueFamilyIndex;
	OutBarrier.dstQueueFamilyIndex = InBarrier.dstQueueFamilyIndex;
	OutBarrier.buffer = InBarrier.buffer;
	OutBarrier.offset = InBarrier.offset;
	OutBarrier.size = InBarrier.size;
}

static void DowngradeBarrier(VkImageMemoryBarrier& OutBarrier, const VkImageMemoryBarrier2& InBarrier)
{
	OutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	OutBarrier.pNext = InBarrier.pNext;
	OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
	OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
	OutBarrier.oldLayout = InBarrier.oldLayout;
	OutBarrier.newLayout = InBarrier.newLayout;
	OutBarrier.srcQueueFamilyIndex = InBarrier.srcQueueFamilyIndex;
	OutBarrier.dstQueueFamilyIndex = InBarrier.dstQueueFamilyIndex;
	OutBarrier.image = InBarrier.image;
	OutBarrier.subresourceRange = InBarrier.subresourceRange;
}

template <typename DstArrayType, typename SrcArrayType>
static void DowngradeBarrierArray(DstArrayType& TargetArray, const SrcArrayType& SrcArray, VkPipelineStageFlags& MergedSrcStageMask, VkPipelineStageFlags& MergedDstStageMask)
{
	TargetArray.Reserve(TargetArray.Num() + SrcArray.Num());
	for (const auto& SrcBarrier : SrcArray)
	{
		auto& DstBarrier = TargetArray.AddDefaulted_GetRef();
		DowngradeBarrier(DstBarrier, SrcBarrier);
		MergedSrcStageMask |= SrcBarrier.srcStageMask;
		MergedDstStageMask |= SrcBarrier.dstStageMask;
	}
}

// Legacy manual barriers inside the RHI with FVulkanPipelineBarrier don't have access to tracking, assume same layout for both aspects
template <typename BarrierArrayType>
static void MergeDepthStencilLayouts(BarrierArrayType& TargetArray)
{
	for (auto& Barrier : TargetArray)
	{
		if (VKHasAnyFlags(Barrier.subresourceRange.aspectMask, (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))
		{
			if (Barrier.newLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
			{
				Barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else if (Barrier.newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
			{
				Barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			if (Barrier.oldLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
			{
				Barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else if (Barrier.oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
			{
				Barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
		}
	}
}

// The transition processor takes RHITransitions and transforms them into Vulkan barriers (of the template type)
template<typename MemoryBarrierType, typename BufferBarrierType, typename ImageBarrierType>
class FTransitionProcessor
{
public:

	FTransitionProcessor(FVulkanCommandListContext& InContext, bool InIsBeginTransition)
		: Context(InContext)
		, IsBeginTransition(InIsBeginTransition)
	{}

	void Process(TArrayView<const FRHITransition*>& Transitions)
	{
		for (const FRHITransition* Transition : Transitions)
		{
			const FVulkanPipelineBarrier* Data = Transition->GetPrivateData<FVulkanPipelineBarrier>();

			const bool IsCrossPipe = (Data->SrcPipelines != Data->DstPipelines);

			// We only care about cross-pipe transitions when we begin transitions
			if (IsBeginTransition && !IsCrossPipe)
			{
				continue;
			}

			ValidateQueue(Data);

			const int32 BufferBatchStartIndex = BufferBarriers.Num();
			const int32 ImageBatchStartIndex = ImageBarriers.Num();
			AppendBarriers(Data);

			// Patch the src/dst access mask for acquire/release on queue ownership transfers
			if (IsCrossPipe)
			{
				PatchCrossPipeTransitions(BufferBarriers, BufferBatchStartIndex);
				PatchCrossPipeTransitions(ImageBarriers, ImageBatchStartIndex);
			}

			// Set the images for the barriers from the texture in the extra data and notify of layout changes
			for (int32 Index = 0; Index < Data->ImageBarrierExtras.Num(); ++Index)
			{
				const FVulkanPipelineBarrier::ImageBarrierExtraData* RemainingExtras = &Data->ImageBarrierExtras[Index];
				FVulkanTexture* Texture = RemainingExtras->BaseTexture;
				check(Texture->Image != VK_NULL_HANDLE || !Texture->IsImageOwner());  // coming in, we should always have an image, except backbuffer with r.Vulkan.DelayAcquireBackBuffer=2

				const int32 TargetIndex = ImageBatchStartIndex + Index;
				ImageBarrierType* RemainingBarriers = &ImageBarriers[TargetIndex];

				// EndTransition handles layout transition notifications
				if (!IsBeginTransition)
				{
					Texture->OnLayoutTransition(Context, RemainingBarriers->newLayout);
				}

				RemainingBarriers->image = Texture->Image;

				if ((RemainingBarriers->image != VK_NULL_HANDLE) && (RemainingBarriers->subresourceRange.aspectMask != 0))
				{
					const int32 RemainingCount = Data->ImageBarrierExtras.Num() - Index;
					ApplyTracking(TargetIndex, RemainingExtras, RemainingCount, IsCrossPipe);
				}
			}

			// Loop again to remove invalidated entries
			for (int32 DstIndex = ImageBatchStartIndex; DstIndex < ImageBarriers.Num();)
			{
				// If Texture is a backbuffer, OnLayoutTransition normally updates the image handle. However, if the viewport got invalidated during the
				// OnLayoutTransition call, we may end up with a null handle, which is fine.  Merged Depth and Stencil transitions will also result in
				// in null aspectMask for the extra transition.  Either is fine, just remove the transition.
				if ((ImageBarriers[DstIndex].image == VK_NULL_HANDLE) || ((ImageBarriers[DstIndex].subresourceRange.aspectMask == 0)))
				{
					ImageBarriers.RemoveAtSwap(DstIndex);
				}
				else
				{
					++DstIndex;
				}
			}

			FinishBatch(Data->SrcPipelines, Data->DstPipelines, BufferBatchStartIndex, ImageBatchStartIndex);
		}

		FinishAll();
	}

private:
	void AppendBarriers(const FVulkanPipelineBarrier* Data);
	void ApplyTracking(uint32 TargetIndex, const FVulkanPipelineBarrier::ImageBarrierExtraData* RemainingExtras, int32 RemainingCount, bool IsCrossPipe);
	void FinishBatch(ERHIPipeline SrcPipelines, ERHIPipeline DstPipelines, int32 BufferBatchStartIndex, int32 ImageBatchStartIndex);
	void FinishAll();

	// Patches for acquire/release of resources during queue ownership transfers
	// For async compute, sanitize the stage and access flags to what is supported by the queue
	template<typename ArrayType>
	void PatchCrossPipeTransitions(ArrayType& Barriers, int32 StartIndex)
	{
		for (int32 Index = StartIndex; Index < Barriers.Num(); ++Index)
		{
			auto& Barrier = Barriers[Index];

			if (Barrier.srcQueueFamilyIndex != Barrier.dstQueueFamilyIndex)
			{
				if (IsBeginTransition)
				{
					Barrier.dstAccessMask = 0; // Release resource from current queue.
				}
				else
				{
					Barrier.srcAccessMask = 0; // Acquire resource on current queue.
				}
			}
		}
	}

	void ValidateQueue(const FVulkanPipelineBarrier* Data)
	{
#if DO_GUARD_SLOW
		ERHIPipeline RequestedPipeline = IsBeginTransition ? Data->SrcPipelines : Data->DstPipelines;
		const TCHAR* RequestedPipelineName = IsBeginTransition ? TEXT("SRC") : TEXT("DST");
		switch (RequestedPipeline)
		{
		case ERHIPipeline::Graphics:
			checkf(!Context.GetDevice()->IsRealAsyncComputeContext(&Context), TEXT("The %s pipeline for this transition is the Graphics queue, but it's submitted on the AsyncCompute queue."), IsBeginTransition ? TEXT("BEGIN") : TEXT("END"));
			break;
		case ERHIPipeline::AsyncCompute:
			checkf(Context.GetDevice()->IsRealAsyncComputeContext(&Context), TEXT("The %s pipeline for this transition is the AsyncCompute queue, but it's submitted on the Graphics queue."), IsBeginTransition ? TEXT("BEGIN") : TEXT("END"));
			break;
		default:
			checkNoEntry();
			break;
		}
#endif
	}

	FVulkanCommandListContext& Context;
	const bool IsBeginTransition;

	VkPipelineStageFlags MergedSrcStageMask = 0;
	VkPipelineStageFlags MergedDstStageMask = 0;

	TArray<MemoryBarrierType> MemoryBarriers;
	TArray<BufferBarrierType> BufferBarriers;
	TArray<ImageBarrierType> ImageBarriers;
};



// *** FTransitionProcessor for Original Barriers ***

template <> 
void FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier>::FinishBatch(ERHIPipeline SrcPipelines, ERHIPipeline DstPipelines, int32 BufferBatchStartIndex, int32 ImageBatchStartIndex)
{
	if ((MemoryBarriers.Num() == 0) && (BufferBarriers.Num() == 0) && (ImageBarriers.Num() == 0))
	{
		return;
	}

	if (SrcPipelines == ERHIPipeline::AsyncCompute)
	{
		MergedSrcStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
	}
	if (DstPipelines == ERHIPipeline::AsyncCompute)
	{
		MergedDstStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
	}

	// Submit merged stage masks with arrays of barriers
	VulkanRHI::vkCmdPipelineBarrier(Context.GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), MergedSrcStageMask, MergedDstStageMask, 0, MemoryBarriers.Num(), MemoryBarriers.GetData(), BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
	MemoryBarriers.Reset();
	BufferBarriers.Reset();
	ImageBarriers.Reset();
	MergedSrcStageMask = 0;
	MergedDstStageMask = 0;
}

template <> 
void FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier>::FinishAll()
{ 
	// Non-Sync2 already submitted each batch
}

template <>
void FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier>::AppendBarriers(const FVulkanPipelineBarrier* Data)
{
	DowngradeBarrierArray(MemoryBarriers, Data->MemoryBarriers, MergedSrcStageMask, MergedDstStageMask);
	DowngradeBarrierArray(BufferBarriers, Data->BufferBarriers, MergedSrcStageMask, MergedDstStageMask);
	DowngradeBarrierArray(ImageBarriers, Data->ImageBarriers, MergedSrcStageMask, MergedDstStageMask);
}

template <>
void FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier>::ApplyTracking(uint32_t TargetIndex, const FVulkanPipelineBarrier::ImageBarrierExtraData* RemainingExtras, int32 RemainingCount, bool IsCrossPipe)
{
	FVulkanLayoutManager& LayoutManager = Context.GetCommandBufferManager()->GetActiveCmdBuffer()->GetLayoutManager();

	VkImageMemoryBarrier* ImageBarrier = &ImageBarriers[TargetIndex];
	FVulkanTexture* Texture = RemainingExtras->BaseTexture;
	check(Texture->Image == ImageBarrier->image);

	// Get a copy of the current layout, we will alter it and feed it back in the manager
	FVulkanImageLayout Layout = *LayoutManager.GetFullLayout(*Texture, true);

	check((Layout.NumMips == Texture->GetNumMips()) && (Layout.NumLayers == Texture->GetNumberOfArrayLevels()));
	check((ImageBarrier->subresourceRange.baseArrayLayer + Layout.GetSubresRangeLayerCount(ImageBarrier->subresourceRange)) <= Layout.NumLayers);
	check((ImageBarrier->subresourceRange.baseMipLevel + Layout.GetSubresRangeMipCount(ImageBarrier->subresourceRange)) <= Layout.NumMips);
	check(ImageBarrier->newLayout != VK_IMAGE_LAYOUT_UNDEFINED);

	// For Depth/Stencil formats where only one of the aspects is transitioned, look ahead for other barriers on the same resource
	if (Texture->IsDepthOrStencilAspect() && (Texture->GetFullAspectMask() != ImageBarrier->subresourceRange.aspectMask))
	{
		check(VKHasAnyFlags(ImageBarrier->subresourceRange.aspectMask, VK_IMAGE_ASPECT_DEPTH_BIT |VK_IMAGE_ASPECT_STENCIL_BIT));
		if ((ImageBarrier->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (!RemainingExtras->IsAliasingBarrier))
		{
			ImageBarrier->oldLayout = Layout.GetSubresLayout(ImageBarrier->subresourceRange.baseArrayLayer, ImageBarrier->subresourceRange.baseMipLevel, (VkImageAspectFlagBits)ImageBarrier->subresourceRange.aspectMask);
		}

		const VkImageAspectFlagBits OtherAspectMask = (VkImageAspectFlagBits)(Texture->GetFullAspectMask() ^ ImageBarrier->subresourceRange.aspectMask);
		VkImageLayout OtherAspectOldLayout = Layout.GetSubresLayout(ImageBarrier->subresourceRange.baseArrayLayer, ImageBarrier->subresourceRange.baseMipLevel, OtherAspectMask);
		VkImageLayout OtherAspectNewLayout = OtherAspectOldLayout;
		for (int32 OtherBarrierIndex = 0; OtherBarrierIndex < RemainingCount; ++OtherBarrierIndex)
		{
			VkImageMemoryBarrier& OtherImageBarrier = ImageBarriers[TargetIndex+OtherBarrierIndex];
			FVulkanTexture* OtherTexture = RemainingExtras[OtherBarrierIndex].BaseTexture;
			if ((OtherTexture->Image == ImageBarrier->image) && (OtherImageBarrier.subresourceRange.aspectMask == OtherAspectMask))
			{
				check(ImageBarrier->subresourceRange.baseArrayLayer == OtherImageBarrier.subresourceRange.baseArrayLayer);
				check(ImageBarrier->subresourceRange.baseMipLevel == OtherImageBarrier.subresourceRange.baseMipLevel);

				OtherAspectNewLayout = OtherImageBarrier.newLayout;
				if ((OtherImageBarrier.oldLayout != VK_IMAGE_LAYOUT_UNDEFINED) || RemainingExtras[OtherBarrierIndex].IsAliasingBarrier)
				{
					OtherAspectOldLayout = OtherImageBarrier.oldLayout;
				}

				Layout.Set(OtherImageBarrier.newLayout, OtherImageBarrier.subresourceRange);

				// Make it invalid so that it gets removed when we reach it
				OtherImageBarrier.subresourceRange.aspectMask = 0;

				break;
			}
		}

		Layout.Set(ImageBarrier->newLayout, ImageBarrier->subresourceRange);

		// Merge the layout with its other half and set it in the barrier
		if (OtherAspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
		{
			ImageBarrier->oldLayout = GetMergedDepthStencilLayout(ImageBarrier->oldLayout, OtherAspectOldLayout);
			ImageBarrier->newLayout = GetMergedDepthStencilLayout(ImageBarrier->newLayout, OtherAspectNewLayout);
		}
		else
		{
			ImageBarrier->oldLayout = GetMergedDepthStencilLayout(OtherAspectOldLayout, ImageBarrier->oldLayout);
			ImageBarrier->newLayout = GetMergedDepthStencilLayout(OtherAspectNewLayout, ImageBarrier->newLayout);
		}
        ImageBarrier->subresourceRange.aspectMask |= OtherAspectMask;

		// Once we're done downgrading the barrier, make sure there are no sync2 states left
		check((ImageBarrier->oldLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) && (ImageBarrier->oldLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL));
		check((ImageBarrier->newLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) && (ImageBarrier->newLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL));
	}
	else
	{
		if ((ImageBarrier->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (!RemainingExtras->IsAliasingBarrier))
		{
			if (Layout.AreAllSubresourcesSameLayout())
			{
				ImageBarrier->oldLayout = Layout.MainLayout;
				ImageBarrier->srcAccessMask = GetVkAccessMaskForLayout(ImageBarrier->oldLayout);
				MergedSrcStageMask |= GetVkStageFlagsForLayout(ImageBarrier->oldLayout);
			}
			else
			{
				// Slow path, adds one transition per subresource.
				check(!IsCrossPipe);
				AddSubresourceTransitions(ImageBarriers, MergedSrcStageMask, *ImageBarrier, Texture->Image, Layout, ImageBarrier->newLayout);
                // Update the pointer to the current barrier as AddSubresourceTransitions can invalidate it when adding
                ImageBarrier = &ImageBarriers[TargetIndex];
			}
		}
		else
		{
			checkSlow(RemainingExtras->IsAliasingBarrier || Layout.AreSubresourcesSameLayout(ImageBarrier->oldLayout, ImageBarrier->subresourceRange));
		}

		const VkImageLayout TrackedNewLayout = ImageBarrier->newLayout;
		if (Texture->IsDepthOrStencilAspect())
		{
			// The only way we end up here is if the barrier transitions every aspect of the depth(-stencil) texture
			check(Texture->GetFullAspectMask() == ImageBarrier->subresourceRange.aspectMask);
			ImageBarrier->oldLayout = GetMergedDepthStencilLayout(ImageBarrier->oldLayout, ImageBarrier->oldLayout);
			ImageBarrier->newLayout = GetMergedDepthStencilLayout(ImageBarrier->newLayout, ImageBarrier->newLayout);
		}

		MergedDstStageMask |= GetVkStageFlagsForLayout(ImageBarriers[TargetIndex].newLayout);

		if (!IsCrossPipe)
		{
			if (ImageBarrier->oldLayout == ImageBarrier->newLayout)
			{
				// It turns out that we don't need a layout transition after all. We may still need a memory barrier if the
				// previous access was writable.

				if (MemoryBarriers.Num() == 0)
				{
					MemoryBarriers.AddDefaulted();
					ZeroVulkanStruct(MemoryBarriers[0], VK_STRUCTURE_TYPE_MEMORY_BARRIER);
				}

				MemoryBarriers[0].srcAccessMask |= ImageBarrier->srcAccessMask;
				MemoryBarriers[0].dstAccessMask |= ImageBarrier->dstAccessMask;
			}
		}

		// Make sure these new barriers are downgraded (since we're already past that step by the time we apply tracking)
		check((ImageBarrier->oldLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) && (ImageBarrier->oldLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL));
		check((ImageBarrier->newLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) && (ImageBarrier->newLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL));

		Layout.Set(TrackedNewLayout, ImageBarrier->subresourceRange);
	}

	LayoutManager.SetFullLayout(*Texture, Layout);
}



// *** FTransitionProcessor for Sync2 Barriers ***

template <> 
void FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2>::FinishBatch(ERHIPipeline SrcPipelines, ERHIPipeline DstPipelines, int32 BufferBatchStartIndex, int32 ImageBatchStartIndex)
{
	// :todo-jn: clean up

	if (SrcPipelines == ERHIPipeline::AsyncCompute)
	{
		for (int32 Index = BufferBatchStartIndex; Index < BufferBarriers.Num(); ++Index)
		{
			VkBufferMemoryBarrier2& BufferBarrier = BufferBarriers[Index];
			BufferBarrier.srcStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
			BufferBarrier.srcAccessMask &= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}

		for (int32 Index = ImageBatchStartIndex; Index < ImageBarriers.Num(); ++Index)
		{
			VkImageMemoryBarrier2& Barrier = ImageBarriers[Index];
			Barrier.srcStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
			Barrier.srcAccessMask &= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}
	}

	if (DstPipelines == ERHIPipeline::AsyncCompute)
	{
		for (int32 Index = BufferBatchStartIndex; Index < BufferBarriers.Num(); ++Index)
		{
			VkBufferMemoryBarrier2& BufferBarrier = BufferBarriers[Index];
			BufferBarrier.dstStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
			BufferBarrier.dstAccessMask &= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}

		for (int32 Index = ImageBatchStartIndex; Index < ImageBarriers.Num(); ++Index)
		{
			VkImageMemoryBarrier2& Barrier = ImageBarriers[Index];
			Barrier.dstStageMask &= Context.GetDevice()->GetComputeQueue()->GetSupportedStageBits();
			Barrier.dstAccessMask &= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		}
	}

	// Sync2 does not submit batches, only one big clump at the end
}

template <>
void FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2>::FinishAll()
{
	if ((MemoryBarriers.Num() == 0) && (BufferBarriers.Num() == 0) && (ImageBarriers.Num() == 0))
	{
		return;
	}

	VkDependencyInfo DependencyInfo;
	DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	DependencyInfo.pNext = nullptr;
	DependencyInfo.dependencyFlags = 0;
	DependencyInfo.memoryBarrierCount = MemoryBarriers.Num();
	DependencyInfo.pMemoryBarriers = MemoryBarriers.GetData();
	DependencyInfo.bufferMemoryBarrierCount = BufferBarriers.Num();
	DependencyInfo.pBufferMemoryBarriers = BufferBarriers.GetData();
	DependencyInfo.imageMemoryBarrierCount = ImageBarriers.Num();
	DependencyInfo.pImageMemoryBarriers = ImageBarriers.GetData();
	VulkanRHI::vkCmdPipelineBarrier2KHR(Context.GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), &DependencyInfo);
}

template <>
void FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2>::AppendBarriers(const FVulkanPipelineBarrier* Data)
{
	MemoryBarriers.Append(Data->MemoryBarriers);
	BufferBarriers.Append(Data->BufferBarriers);
	ImageBarriers.Append(Data->ImageBarriers);
}

template <>
void FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2>::ApplyTracking(uint32 TargetIndex, const FVulkanPipelineBarrier::ImageBarrierExtraData* RemainingExtras, int32 RemainingCount, bool IsCrossPipe)
{
	if (GVulkanAutoCorrectUnknownLayouts)
	{
		VkImageMemoryBarrier2& ImageBarrier = ImageBarriers[TargetIndex];

		if ((ImageBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (!RemainingExtras->IsAliasingBarrier))
		{
			FVulkanLayoutManager& LayoutManager = Context.GetCommandBufferManager()->GetActiveCmdBuffer()->GetLayoutManager();
			FVulkanTexture* Texture = RemainingExtras->BaseTexture;
			check(Texture->Image == ImageBarrier.image);
			const FVulkanImageLayout* Layout = LayoutManager.GetFullLayout(*Texture, false);
			if (Layout)
			{
				if (Layout->AreAllSubresourcesSameLayout())
				{
					ImageBarrier.oldLayout = Layout->MainLayout;
				}
				else
				{
					ForEachAspect(ImageBarrier.subresourceRange.aspectMask, [&](VkImageAspectFlagBits SingleAspect)
						{
							const uint32 LastLayer = ImageBarrier.subresourceRange.baseArrayLayer + ImageBarrier.subresourceRange.layerCount;
							const uint32 LastMip = ImageBarrier.subresourceRange.baseMipLevel + ImageBarrier.subresourceRange.levelCount;
							for (uint32 LayerIndex = ImageBarrier.subresourceRange.baseArrayLayer; LayerIndex < LastLayer; ++LayerIndex)
							{
								for (uint32 MipIndex = ImageBarrier.subresourceRange.baseMipLevel; MipIndex < LastMip; ++MipIndex)
								{
									const VkImageLayout SubresourceLayout = Layout->GetSubresLayout(LayerIndex, MipIndex, SingleAspect);
									check((ImageBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) || (ImageBarrier.oldLayout == SubresourceLayout));
									ImageBarrier.oldLayout = SubresourceLayout;
								}
							}
						});
				}

				ImageBarrier.srcAccessMask = GetVkAccessMaskForLayout(ImageBarrier.oldLayout);
				ImageBarrier.srcStageMask = GetVkStageFlagsForLayout(ImageBarrier.oldLayout);
			}
		}
	}

	// Sync2 only needs to inform tracking of the new layout
	Context.GetCommandBufferManager()->GetActiveCmdBuffer()->GetLayoutManager().SetLayout(*RemainingExtras->BaseTexture, ImageBarriers[TargetIndex].subresourceRange, ImageBarriers[TargetIndex].newLayout);

	// No needs to worry about 'surprise' subresource transition
	// No need to worry about partial depth/stencil layout changes
	// Identical layouts are chill ("Image memory barriers that do not perform an image layout transition can be specified by setting oldLayout equal to newLayout")
}





static TArray<VulkanRHI::FSemaphore*> ExtractTransitionSemaphores(TArrayView<const FRHITransition*>& Transitions, ERHIPipeline SkipSrcPipeline = ERHIPipeline::None)
{
	TArray<VulkanRHI::FSemaphore*> Semaphores;

	for (const FRHITransition* Transition : Transitions)
	{
		const FVulkanPipelineBarrier* Data = Transition->GetPrivateData<FVulkanPipelineBarrier>();
		if (Data->Semaphore == nullptr)
		{
			continue;
		}

		// Avoid unnecessary semaphores when DstPipelines is ERHIPipeline::All
		if ((SkipSrcPipeline != ERHIPipeline::None) && EnumHasAllFlags(Data->DstPipelines, Data->SrcPipelines) && (Data->SrcPipelines == SkipSrcPipeline))
		{
			continue;
		}

		Semaphores.Add(Data->Semaphore);
	}

	return Semaphores;
}


void FVulkanCommandListContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIBeginTransitions, bShowTransitionEvents, TEXT("RHIBeginTransitions"));

	TRACE_CPUPROFILER_EVENT_SCOPE(RHIBeginTransitions);

	if (Device->SupportsParallelRendering())
	{
		FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2> Processor(*this, true);
		Processor.Process(Transitions);
	}
	else
	{
		FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier> Processor(*this, true);
		Processor.Process(Transitions);
	}

	// Signal semaphores
	TArray<VulkanRHI::FSemaphore*> SignalSemaphores = ExtractTransitionSemaphores(Transitions);
	if (SignalSemaphores.Num() > 0)
	{
		CommandBufferManager->SubmitActiveCmdBuffer(SignalSemaphores);
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
	}
}

void FVulkanCommandListContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIEndTransitions, bShowTransitionEvents, TEXT("RHIEndTransitions"));

	TRACE_CPUPROFILER_EVENT_SCOPE(RHIEndTransitions);

	const ERHIPipeline CurrentPipeline = Device->IsRealAsyncComputeContext(this) ? ERHIPipeline::AsyncCompute : ERHIPipeline::Graphics;
	TArray<VulkanRHI::FSemaphore*> WaitSemaphores = ExtractTransitionSemaphores(Transitions, CurrentPipeline);
	if (WaitSemaphores.Num() > 0)
	{
		if (CommandBufferManager->HasPendingActiveCmdBuffer())
		{
			CommandBufferManager->SubmitActiveCmdBuffer();
			CommandBufferManager->PrepareForNewActiveCommandBuffer();
		}

		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		CmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, WaitSemaphores);
	}

	if (Device->SupportsParallelRendering())
	{
		FTransitionProcessor<VkMemoryBarrier2, VkBufferMemoryBarrier2, VkImageMemoryBarrier2> Processor(*this, false);
		Processor.Process(Transitions);
	}
	else
	{
		FTransitionProcessor<VkMemoryBarrier, VkBufferMemoryBarrier, VkImageMemoryBarrier> Processor(*this, false);
		Processor.Process(Transitions);
	}
}


void FVulkanPipelineBarrier::AddMemoryBarrier(VkAccessFlags InSrcAccessFlags, VkAccessFlags InDstAccessFlags, VkPipelineStageFlags InSrcStageMask, VkPipelineStageFlags InDstStageMask)
{
	const VkAccessFlags ReadMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;

	if (MemoryBarriers.Num() == 0)
	{
		VkMemoryBarrier2& NewBarrier = MemoryBarriers.AddDefaulted_GetRef();
		ZeroVulkanStruct(NewBarrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
	}

	// Mash everything into a single barrier
	VkMemoryBarrier2& MemoryBarrier = MemoryBarriers[0];

	// We only need a memory barrier if the previous commands wrote to the buffer. In case of a transition from read, an execution barrier is enough.
	const bool SrcAccessIsRead = ((InSrcAccessFlags & (~ReadMask)) == 0);
	if (!SrcAccessIsRead)
	{
		MemoryBarrier.srcAccessMask |= InSrcAccessFlags;
		MemoryBarrier.dstAccessMask |= InDstAccessFlags;
	}

	MemoryBarrier.srcStageMask |= InSrcStageMask;
	MemoryBarrier.dstStageMask |= InDstStageMask;
}


//
// Methods used when the RHI itself needs to perform a layout transition. The public API functions do not call these,
// they fill in the fields of FVulkanPipelineBarrier using their own logic, based on the ERHIAccess flags.
//

void FVulkanPipelineBarrier::AddFullImageLayoutTransition(const FVulkanTexture& Texture, VkImageLayout SrcLayout, VkImageLayout DstLayout)
{
	const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcLayout);
	const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);

	const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
	const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

	const VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(Texture.GetFullAspectMask());
	if (Texture.IsDepthOrStencilAspect())
	{
		SrcLayout = GetMergedDepthStencilLayout(SrcLayout, SrcLayout);
		DstLayout = GetMergedDepthStencilLayout(DstLayout, DstLayout);
	}

	VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
	SetupImageBarrier(ImgBarrier, Texture.Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange& SubresourceRange)
{
	const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcLayout);
	const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);

	const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
	const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

	VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
	SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, const FVulkanImageLayout& SrcLayout, VkImageLayout DstLayout)
{
	if (SrcLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, SrcLayout.MainLayout, DstLayout, MakeSubresourceRange(AspectMask));
		return;
	}

	const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);
	const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

	ForEachAspect(AspectMask, [&](VkImageAspectFlagBits SingleAspect)
		{
			VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(SingleAspect, 0, 1, 0, 1);
			for (; SubresourceRange.baseArrayLayer < SrcLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
			{
				for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < SrcLayout.NumMips; ++SubresourceRange.baseMipLevel)
				{
					const VkImageLayout SubresourceLayout = SrcLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel, SingleAspect);
					if (SubresourceLayout != DstLayout)
					{
						const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SubresourceLayout);
						const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SubresourceLayout);

						VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
						SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SubresourceLayout, DstLayout, SubresourceRange);
					}
				}
			}
		});
}

void FVulkanPipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout SrcLayout, const FVulkanImageLayout& DstLayout)
{
	if (DstLayout.AreAllSubresourcesSameLayout())
	{
		AddImageLayoutTransition(Image, SrcLayout, DstLayout.MainLayout, MakeSubresourceRange(AspectMask));
		return;
	}

	const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcLayout);
	const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);

	ForEachAspect(AspectMask, [&](VkImageAspectFlagBits SingleAspect)
		{
			VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(SingleAspect, 0, 1, 0, 1);
			for (; SubresourceRange.baseArrayLayer < DstLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
			{
				for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < DstLayout.NumMips; ++SubresourceRange.baseMipLevel)
				{
					const VkImageLayout SubresourceLayout = DstLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel, SingleAspect);
					if (SubresourceLayout != SrcLayout)
					{
						const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(SubresourceLayout);
						const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(SubresourceLayout);

						VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
						SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, SubresourceLayout, SubresourceRange);
					}
				}
			}
		});
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

		ForEachAspect(AspectMask, [&](VkImageAspectFlagBits SingleAspect)
			{
				VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(SingleAspect, 0, 1, 0, 1);
				for (; SubresourceRange.baseArrayLayer < DstLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
				{
					for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < DstLayout.NumMips; ++SubresourceRange.baseMipLevel)
					{
						const VkImageLayout SrcSubresourceLayout = SrcLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel, SingleAspect);
						const VkImageLayout DstSubresourceLayout = DstLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel, SingleAspect);
						if (SrcSubresourceLayout != DstSubresourceLayout)
						{
							const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcSubresourceLayout);
							const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcSubresourceLayout);

							const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstSubresourceLayout);
							const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstSubresourceLayout);

							VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
							SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcSubresourceLayout, DstSubresourceLayout, SubresourceRange);
						}
					}
				}
			});
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

	VkImageMemoryBarrier2& ImgBarrier = ImageBarriers.AddDefaulted_GetRef();
	SetupImageBarrier(ImgBarrier, Surface.Image, ImgSrcStage, ImgDstStage, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);

	InOutLayout = DstLayout;
}

void FVulkanPipelineBarrier::Execute(VkCommandBuffer CmdBuffer)
{
	if (MemoryBarriers.Num() != 0 || BufferBarriers.Num() != 0 || ImageBarriers.Num() != 0)
	{
		VkPipelineStageFlags SrcStageMask = 0;
		VkPipelineStageFlags DstStageMask = 0;

		TArray<VkMemoryBarrier, TInlineAllocator<1>> TempMemoryBarriers;
		DowngradeBarrierArray(TempMemoryBarriers, MemoryBarriers, SrcStageMask, DstStageMask);

		TArray<VkBufferMemoryBarrier> TempBufferBarriers;
		DowngradeBarrierArray(TempBufferBarriers, BufferBarriers, SrcStageMask, DstStageMask);

		TArray<VkImageMemoryBarrier, TInlineAllocator<2>> TempImageBarriers;
		DowngradeBarrierArray(TempImageBarriers, ImageBarriers, SrcStageMask, DstStageMask);
		MergeDepthStencilLayouts(TempImageBarriers);

		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer, SrcStageMask, DstStageMask, 0, TempMemoryBarriers.Num(), TempMemoryBarriers.GetData(), 
			TempBufferBarriers.Num(), TempBufferBarriers.GetData(), TempImageBarriers.Num(), TempImageBarriers.GetData());
	}
}

void FVulkanPipelineBarrier::Execute(FVulkanCmdBuffer* CmdBuffer)
{
	if (MemoryBarriers.Num() != 0 || BufferBarriers.Num() != 0 || ImageBarriers.Num() != 0)
	{
		if (CmdBuffer->GetDevice()->SupportsParallelRendering())
		{
			VkDependencyInfo DependencyInfo;
			DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			DependencyInfo.pNext = nullptr;
			DependencyInfo.dependencyFlags = 0;
			DependencyInfo.memoryBarrierCount = MemoryBarriers.Num();
			DependencyInfo.pMemoryBarriers = MemoryBarriers.GetData();
			DependencyInfo.bufferMemoryBarrierCount = BufferBarriers.Num();
			DependencyInfo.pBufferMemoryBarriers = BufferBarriers.GetData();
			DependencyInfo.imageMemoryBarrierCount = ImageBarriers.Num();
			DependencyInfo.pImageMemoryBarriers = ImageBarriers.GetData();
			VulkanRHI::vkCmdPipelineBarrier2KHR(CmdBuffer->GetHandle(), &DependencyInfo);
		}
		else
		{
			// Call the original execute with older types
			Execute(CmdBuffer->GetHandle());
		}
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
void VulkanSetImageLayout(FVulkanCmdBuffer* CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange)
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

	const uint32 FirstPlane = (SubresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0;
	const uint32 LastPlane = (SubresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes : 1;

	const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
	const uint32 LastLayer = FirstLayer + GetSubresRangeLayerCount(SubresourceRange);

	const uint32 FirstMip = SubresourceRange.baseMipLevel;
	const uint32 LastMip = FirstMip + GetSubresRangeMipCount(SubresourceRange);

	for (uint32 PlaneIdx = FirstPlane; PlaneIdx < LastPlane; ++PlaneIdx)
	{
		for (uint32 LayerIdx = FirstLayer; LayerIdx < LastLayer; ++LayerIdx)
		{
			for (uint32 MipIdx = FirstMip; MipIdx < LastMip; ++MipIdx)
			{
				if (SubresLayouts[(PlaneIdx * NumLayers * NumMips) + (LayerIdx * NumMips) + MipIdx] != Layout)
				{
					return false;
				}
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

	const VkImageLayout Layout = SubresLayouts[0];
	for (uint32 i = 1; i < NumPlanes * NumLayers * NumMips; ++i)
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
	checkf(
		(Layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL) &&
		(Layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL) &&
		(Layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) &&
		(Layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
		TEXT("Layout tracking should always use separate depth and stencil layouts.")
	);

	const uint32 FirstPlane = (SubresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0;
	const uint32 LastPlane = (SubresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes : 1;

	const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
	const uint32 LayerCount = GetSubresRangeLayerCount(SubresourceRange);

	const uint32 FirstMip = SubresourceRange.baseMipLevel;
	const uint32 MipCount = GetSubresRangeMipCount(SubresourceRange);

	if (FirstPlane == 0 && LastPlane == NumPlanes &&
		FirstLayer == 0 && LayerCount == NumLayers && 
		FirstMip == 0 && MipCount == NumMips)
	{
		// We're setting the entire resource to the same layout.
		MainLayout = Layout;
		SubresLayouts.Reset();
		return;
	}

	if (SubresLayouts.Num() == 0)
	{
		const uint32 SubresLayoutCount = NumPlanes * NumLayers * NumMips;
		SubresLayouts.SetNum(SubresLayoutCount);
		for (uint32 i = 0; i < SubresLayoutCount; ++i)
		{
			SubresLayouts[i] = MainLayout;
		}
	}

	for (uint32 Plane = FirstPlane; Plane < LastPlane; ++Plane)
	{
		for (uint32 Layer = FirstLayer; Layer < FirstLayer + LayerCount; ++Layer)
		{
			for (uint32 Mip = FirstMip; Mip < FirstMip + MipCount; ++Mip)
			{
				SubresLayouts[Plane * (NumLayers * NumMips) + Layer * NumMips + Mip] = Layout;
			}
		}
	}

	// It's possible we've just set all the subresources to the same layout. If that's the case, get rid of the
	// subresource info and set the main layout appropriatedly.
	CollapseSubresLayoutsIfSame();
}

VkImageLayout FVulkanLayoutManager::GetDefaultLayout(FVulkanCmdBuffer* CmdBuffer, const FVulkanTexture& VulkanTexture, ERHIAccess DesiredAccess)
{
	switch (DesiredAccess)
	{
	case ERHIAccess::SRVCompute:
	case ERHIAccess::SRVGraphics:
	case ERHIAccess::SRVMask:
	{
		if (VulkanTexture.IsDepthOrStencilAspect())
		{
			const bool bSupportsParallelRendering = CmdBuffer->GetDevice()->SupportsParallelRendering();
			if (bSupportsParallelRendering)
			{
				return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

			}
			else
			{
				// The only layout that can't be hardcoded...  Even if we only use depth, we need to know stencil
				FVulkanLayoutManager& LayoutMgr = CmdBuffer->GetLayoutManager();
				const VkImageLayout DepthLayout = LayoutMgr.GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
				const VkImageLayout StencilLayout = LayoutMgr.GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
				return GetMergedDepthStencilLayout(DepthLayout, StencilLayout);
			}
		}
		else
		{
			return VulkanTexture.SupportsSampling() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
		}
	}

	case ERHIAccess::UAVCompute:
	case ERHIAccess::UAVGraphics:
	case ERHIAccess::UAVMask: return VK_IMAGE_LAYOUT_GENERAL;

	case ERHIAccess::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case ERHIAccess::CopyDest: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	case ERHIAccess::DSVRead: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case ERHIAccess::DSVWrite: return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

	case ERHIAccess::ShadingRateSource:
	{
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}
		else if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}
	}

	default:
		checkNoEntry();
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

VkImageLayout FVulkanLayoutManager::SetExpectedLayout(FVulkanCmdBuffer* CmdBuffer, const FVulkanTexture& VulkanTexture, ERHIAccess DesiredAccess)
{
	const VkImageLayout ExpectedLayout = GetDefaultLayout(CmdBuffer, VulkanTexture, DesiredAccess);
	VkImageLayout PreviousLayout = ExpectedLayout;

	FVulkanLayoutManager& LayoutMgr = CmdBuffer->GetLayoutManager();

	// This code path is not safe for multi-threaded code
	if (!LayoutMgr.bWriteOnly || GVulkanAutoCorrectExpectedLayouts)
	{
		const FVulkanImageLayout* SrcLayout = LayoutMgr.Layouts.Find(VulkanTexture.Image);

		if (!SrcLayout)
		{
			FVulkanLayoutManager& QueueLayoutMgr = CmdBuffer->GetOwner()->GetMgr().GetCommandListContext()->GetQueue()->GetLayoutManager();
			SrcLayout = QueueLayoutMgr.Layouts.Find(VulkanTexture.Image);
		}

		if (SrcLayout && (!SrcLayout->AreAllSubresourcesSameLayout() || (SrcLayout->MainLayout != ExpectedLayout)))
		{
			FVulkanPipelineBarrier Barrier;
			if (VulkanTexture.IsDepthOrStencilAspect() && (SrcLayout->NumPlanes > 1) && !LayoutMgr.bWriteOnly)
			{
				const VkImageLayout MergedLayout = GetMergedDepthStencilLayout(SrcLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_DEPTH_BIT), SrcLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_STENCIL_BIT));
				Barrier.AddImageLayoutTransition(VulkanTexture.Image, MergedLayout, ExpectedLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VulkanTexture.GetFullAspectMask()));
				PreviousLayout = MergedLayout;
			}
			else
			{
				Barrier.AddImageLayoutTransition(VulkanTexture.Image, VulkanTexture.GetFullAspectMask(), *SrcLayout, ExpectedLayout);
				check(SrcLayout->AreAllSubresourcesSameLayout());
				PreviousLayout = SrcLayout->MainLayout;
			}
			Barrier.Execute(CmdBuffer);
		}
	}

	return PreviousLayout;
}

// todo-jn: these function are called hints, but it more of a temporary hack to access depth/stencil information for render passes
VkImageLayout FVulkanLayoutManager::GetDepthStencilHint(const FVulkanTexture& VulkanTexture, VkImageAspectFlagBits AspectBit)
{
	check(AspectBit == VK_IMAGE_ASPECT_DEPTH_BIT || AspectBit == VK_IMAGE_ASPECT_STENCIL_BIT);
	FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture.Image);
	if (Layout)
	{
		return Layout->GetSubresLayout(0, 0, AspectBit);
	}
	else if (Fallback)
	{
		return Fallback->GetDepthStencilHint(VulkanTexture, AspectBit);
	}
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

void FVulkanLayoutManager::NotifyDeletedImage(VkImage Image)
{
	Layouts.Remove(Image);
}

void FVulkanLayoutManager::TransferTo(FVulkanLayoutManager& Destination)
{
	if (Layouts.Num())
	{
		Destination.Layouts.Append(Layouts);
		Layouts.Empty();
	}
}
