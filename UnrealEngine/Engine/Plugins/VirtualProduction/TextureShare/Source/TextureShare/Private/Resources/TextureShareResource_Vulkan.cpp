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

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareResourceHelpers
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
using namespace TextureShareResourceHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::VulkanRegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest)
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
					if (VulkanResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc(), DeviceVulkanContext, VulkanResource, ResourceDesc, ResourceHandle))
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

bool FTextureShareResource::VulkanReleaseTextureShareHandle()
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

					return VulkanResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc(), DeviceVulkanContext, VulkanResource);
				}
			}
		}
	}

	return false;
}
#endif
