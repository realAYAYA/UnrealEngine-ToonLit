// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_VulkanExtensions.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMDPrivateRHI.h"
#include "OculusHMDModule.h"

namespace OculusHMD
{


//-------------------------------------------------------------------------------------------------
// FVulkanExtensions
//-------------------------------------------------------------------------------------------------

bool FVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
	TArray<VkExtensionProperties> Properties = GetIVulkanDynamicRHI()->RHIGetAllInstanceExtensions();

	TArray<const char*> Extensions;
	{
		int32 ExtensionCount = 0;
		FOculusHMDModule::GetPluginWrapper().GetInstanceExtensionsVk(nullptr, &ExtensionCount);
		Extensions.SetNum(ExtensionCount);
		FOculusHMDModule::GetPluginWrapper().GetInstanceExtensionsVk(Extensions.GetData(), &ExtensionCount);
	}

	int32 ExtensionsFound = 0;
	for (int32 ExtensionIndex = 0; ExtensionIndex < Extensions.Num(); ExtensionIndex++)
	{
		for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, Extensions[ExtensionIndex]))
			{
				Out.Add(Extensions[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == Extensions.Num();
#endif
	return true;
}


bool FVulkanExtensions::GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
	TArray<VkExtensionProperties> Properties = GetIVulkanDynamicRHI()->RHIGetAllDeviceExtensions((VkPhysicalDevice)pPhysicalDevice);

	TArray<const char*> Extensions;
	{
		int32 ExtensionCount = 0;
		FOculusHMDModule::GetPluginWrapper().GetDeviceExtensionsVk(nullptr, &ExtensionCount);
		Extensions.SetNum(ExtensionCount);
		FOculusHMDModule::GetPluginWrapper().GetDeviceExtensionsVk(Extensions.GetData(), &ExtensionCount);
	}

	int32 ExtensionsFound = 0;
	for (int32 ExtensionIndex = 0; ExtensionIndex < Extensions.Num(); ExtensionIndex++)
	{
		for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, Extensions[ExtensionIndex]))
			{
				Out.Add(Extensions[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == Extensions.Num();
#endif
	return true;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS