// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.cpp: Vulkan state implementation.
=============================================================================*/

#include "VulkanRenderpass.h"

VkRenderPass CreateVulkanRenderPass(FVulkanDevice& InDevice, const FVulkanRenderTargetLayout& RTLayout)
{
	VkRenderPass OutRenderpass;

#if VULKAN_SUPPORTS_RENDERPASS2
	if (InDevice.GetOptionalExtensions().HasKHRRenderPass2)
	{
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription2>, FVulkanSubpassDependency<VkSubpassDependency2>, FVulkanAttachmentReference<VkAttachmentReference2>, FVulkanAttachmentDescription<VkAttachmentDescription2>, FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2>> Creator(InDevice);
		OutRenderpass = Creator.Create(RTLayout);
	}
	else
#endif
	{
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription>, FVulkanSubpassDependency<VkSubpassDependency>, FVulkanAttachmentReference<VkAttachmentReference>, FVulkanAttachmentDescription<VkAttachmentDescription>, FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>> Creator(InDevice);
		OutRenderpass = Creator.Create(RTLayout);
	}

	return OutRenderpass;
}

