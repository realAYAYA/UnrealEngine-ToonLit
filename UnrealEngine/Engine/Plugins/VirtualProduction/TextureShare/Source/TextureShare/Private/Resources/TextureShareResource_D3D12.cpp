// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"
#include "ITextureShareCoreD3D12ResourcesCache.h"

#include "RHI.h"
#include "RenderResource.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d12.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareResourceHelpers
{
	static TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> GetD3D12ResourcesCache()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPI = ITextureShareCore::Get().GetTextureShareCoreAPI();
		return TextureShareCoreAPI.GetD3D12ResourcesCache();
	}

};
using namespace TextureShareResourceHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::D3D12RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest)
{
	if (GDynamicRHI && TextureRHI.IsValid())
	{
		if (ID3D12Device* D3D12DevicePtr = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice()))
		{
			if (ID3D12Resource* D3D12ResourcePtr = (ID3D12Resource*)(TextureRHI->GetNativeResource()))
			{
				TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> D3D12ResourcesCache = GetD3D12ResourcesCache();
				if (D3D12ResourcesCache.IsValid())
				{
					FTextureShareCoreResourceHandle ResourceHandle;
					if (D3D12ResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc(), D3D12DevicePtr, D3D12ResourcePtr, ResourceDesc, ResourceHandle))
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

bool FTextureShareResource::D3D12ReleaseTextureShareHandle()
{
	if (GDynamicRHI && TextureRHI.IsValid())
	{
		if (ID3D12Device* D3D12DevicePtr = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice()))
		{
			if (ID3D12Resource* D3D12ResourcePtr = (ID3D12Resource*)(TextureRHI->GetNativeResource()))
			{
				TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> D3D12ResourcesCache = GetD3D12ResourcesCache();
				if (D3D12ResourcesCache.IsValid())
				{
					return D3D12ResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc(), D3D12ResourcePtr);
				}
			}
		}
	}

	return false;
}
