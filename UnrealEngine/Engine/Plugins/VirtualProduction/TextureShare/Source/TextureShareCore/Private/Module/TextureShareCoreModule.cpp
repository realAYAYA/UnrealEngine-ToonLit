// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareCoreModule.h"
#include "Module/TextureShareCoreLog.h"

#include "Core/TextureShareCore.h"

#ifdef TEXTURESHARECORE_INITIALIZEVULKANEXTENSIONS
#include "VulkanRHIBridge.h"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreModule
//////////////////////////////////////////////////////////////////////////////////////////////
ITextureShareCoreAPI& FTextureShareCoreModule::GetTextureShareCoreAPI()
{
	if (!TextureShareCoreAPI.IsValid())
	{
		TextureShareCoreAPI = MakeUnique<FTextureShareCore>();
		TextureShareCoreAPI->BeginSession();
	}

	return *TextureShareCoreAPI;
}

void FTextureShareCoreModule::StartupModule()
{
	UE_LOG(LogTextureShareCore, Log, TEXT("TextureShareCore module startup"));

#ifdef TEXTURESHARECORE_INITIALIZEVULKANEXTENSIONS
	{
		// Initialize Vulkan extensions:
		UE_LOG(LogTextureShareCore, Log, TEXT("TextureShareCore: Initialize Vulkan extensions"));

		const TArray<const ANSICHAR*> DeviceExtensions = {
			VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
			VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
		};
		const TArray<const ANSICHAR*> DeviceLayers = {
		};

		const TArray<const ANSICHAR*> InstanceExtensions = {
			VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME
		};
		const TArray<const ANSICHAR*> InstanceLayers = {
		};

		VulkanRHIBridge::AddEnabledDeviceExtensionsAndLayers(DeviceExtensions, DeviceLayers);
		VulkanRHIBridge::AddEnabledInstanceExtensionsAndLayers(InstanceExtensions, InstanceLayers);
	}
#endif
}

void FTextureShareCoreModule::ShutdownModule()
{
	UE_LOG(LogTextureShareCore, Log, TEXT("TextureShareCore module shutdown"));

	ShutdownModuleImpl();
}

void FTextureShareCoreModule::ShutdownModuleImpl()
{
	if (TextureShareCoreAPI.IsValid())
	{
		TextureShareCoreAPI->EndSession();
		TextureShareCoreAPI.Reset();
	}
}

IMPLEMENT_MODULE(FTextureShareCoreModule, TextureShareCore);
