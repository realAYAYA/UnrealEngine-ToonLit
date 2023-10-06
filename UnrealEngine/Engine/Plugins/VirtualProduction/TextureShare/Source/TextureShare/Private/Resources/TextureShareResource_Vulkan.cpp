// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"

#if TEXTURESHARE_VULKAN

#include "Containers/TextureShareContainers.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"
#include "ITextureShareCoreVulkanResourcesCache.h"

#include "RHI.h"
#include "RenderResource.h"
#include "VulkanRHIPrivate.h"

namespace UE::TextureShare::Resource_Vulkan
{
	static TSharedPtr<ITextureShareCoreVulkanResourcesCache, ESPMode::ThreadSafe> GetVulkanResourcesCache()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPI = ITextureShareCore::Get().GetTextureShareCoreAPI();

		return TextureShareCoreAPI.GetVulkanResourcesCache();
	}

	static FTextureShareDeviceVulkanContext GetDeviceVulkanContext()
	{
		if (FVulkanDynamicRHI* DynamicRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI))
		{
			return FTextureShareDeviceVulkanContext(DynamicRHI->GetInstance(), DynamicRHI->GetDevice()->GetPhysicalHandle(), DynamicRHI->GetDevice()->GetInstanceHandle());
		}

		return FTextureShareDeviceVulkanContext();
	}
};
using namespace UE::TextureShare::Resource_Vulkan;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::VulkanRegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	if (TextureRHI.IsValid())
	{
		TSharedPtr<ITextureShareCoreVulkanResourcesCache, ESPMode::ThreadSafe> VulkanResourcesCache = GetVulkanResourcesCache();
		if (VulkanResourcesCache.IsValid())
		{
			FTextureShareDeviceVulkanContext DeviceVulkanContext = GetDeviceVulkanContext();
			if (DeviceVulkanContext.IsValid())
			{
				if (FVulkanTexture2D* VulkanTexturePtr = static_cast<FVulkanTexture2D*>(TextureRHI.GetReference()))
				{
					const FTextureShareDeviceVulkanResource VulkanResource(VulkanTexturePtr->Surface.GetAllocationHandle(), VulkanTexturePtr->Surface.GetAllocationOffset());

					FTextureShareCoreResourceHandle ResourceHandle;
					if (VulkanResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc_RenderThread(), DeviceVulkanContext, VulkanResource, ResourceDesc, ResourceHandle))
					{
						CoreObject->GetProxyData_RenderThread().ResourceHandles.Add(ResourceHandle);

						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FTextureShareResource::VulkanReleaseTextureShareHandle_RenderThread()
{
	if (TextureRHI.IsValid())
	{
		TSharedPtr<ITextureShareCoreVulkanResourcesCache, ESPMode::ThreadSafe> VulkanResourcesCache = GetVulkanResourcesCache();
		if (VulkanResourcesCache.IsValid())
		{
			FTextureShareDeviceVulkanContext DeviceVulkanContext = GetDeviceVulkanContext();
			if (DeviceVulkanContext.IsValid())
			{
				if (FVulkanTexture2D* VulkanTexturePtr = static_cast<FVulkanTexture2D*>(TextureRHI.GetReference()))
				{
					const FTextureShareDeviceVulkanResource VulkanResource(VulkanTexturePtr->Surface.GetAllocationHandle(), VulkanTexturePtr->Surface.GetAllocationOffset());

					return VulkanResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc_RenderThread(), DeviceVulkanContext, VulkanResource);
				}
			}
		}
	}

	return false;
}
#endif
