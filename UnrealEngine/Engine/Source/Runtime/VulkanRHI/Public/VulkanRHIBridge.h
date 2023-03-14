// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRHIBridge.h: Utils to interact with the inner RHI.
=============================================================================*/

#pragma once

#include "IVulkanDynamicRHI.h"

class FVulkanDynamicRHI;
class FVulkanDevice;

namespace VulkanRHIBridge
{
	UE_DEPRECATED(5.1, "Please use IVulkanDynamicRHI::AddEnabledInstanceExtensionsAndLayers()")
	inline void AddEnabledInstanceExtensionsAndLayers(const TArray<const ANSICHAR*>& InInstanceExtensions, const TArray<const ANSICHAR*>& InInstanceLayers)
	{
		IVulkanDynamicRHI::AddEnabledInstanceExtensionsAndLayers(InInstanceExtensions, InInstanceLayers);
	}
	
	UE_DEPRECATED(5.1, "Please use IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers()")
	inline void AddEnabledDeviceExtensionsAndLayers(const TArray<const ANSICHAR*>& InDeviceExtensions, const TArray<const ANSICHAR*>& InDeviceLayers)
	{
		IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(InDeviceExtensions, InDeviceLayers);
	}

	UE_DEPRECATED(5.1, "Please use IVulkanDynamicRHI::RHIGetVkInstance()")
	inline uint64 GetInstance(FVulkanDynamicRHI*)
	{
		return (uint64)GetIVulkanDynamicRHI()->RHIGetVkInstance();
	}

	UE_DEPRECATED(5.1, "Use IVulkanDynamicRHI instead of FVulkanDevice directly.")
	VULKANRHI_API FVulkanDevice* GetDevice(FVulkanDynamicRHI*);

	// Returns a VkDevice
	UE_DEPRECATED(5.1, "Please use IVulkanDynamicRHI::RHIGetVkDevice()")
	inline uint64 GetLogicalDevice(FVulkanDevice*)
	{
		return (uint64)GetIVulkanDynamicRHI()->RHIGetVkDevice();
	}

	// Returns a VkDeviceVkPhysicalDevice
	UE_DEPRECATED(5.1, "Please use IVulkanDynamicRHI::RHIGetVkPhysicalDevice()")
	inline uint64 GetPhysicalDevice(FVulkanDevice*)
	{
		return (uint64)GetIVulkanDynamicRHI()->RHIGetVkPhysicalDevice();
	}
}
