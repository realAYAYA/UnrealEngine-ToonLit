// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"
#include "ITextureShareCoreD3D11ResourcesCache.h"

#include "RHI.h"
#include "RenderResource.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d11.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareResourceHelpers
{
	static TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> GetD3D11ResourcesCache()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPI = ITextureShareCore::Get().GetTextureShareCoreAPI();
		return TextureShareCoreAPI.GetD3D11ResourcesCache();
	}

};
using namespace TextureShareResourceHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::D3D11RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest)
{
	if (GD3D11RHI && TextureRHI.IsValid())
	{
		TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> D3D11ResourcesCache = GetD3D11ResourcesCache();
		if (D3D11ResourcesCache.IsValid())
		{
			if (ID3D11Device* D3D11DevicePtr = static_cast<ID3D11Device*>(GD3D11RHI->RHIGetNativeDevice()))
			{
				if (ID3D11Texture2D* D3D11ResourcePtr = (ID3D11Texture2D*)(TextureRHI->GetNativeResource()))
				{
					FTextureShareCoreResourceHandle ResourceHandle;
					if (D3D11ResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc(), D3D11DevicePtr, D3D11ResourcePtr, ResourceDesc, ResourceHandle))
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

bool FTextureShareResource::D3D11ReleaseTextureShareHandle()
{
	if (GD3D11RHI && TextureRHI.IsValid())
	{
		if (ID3D11Texture2D* D3D11ResourcePtr = (ID3D11Texture2D*)(TextureRHI->GetNativeResource()))
		{
			TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> D3D11ResourcesCache = GetD3D11ResourcesCache();
			if (D3D11ResourcesCache.IsValid())
			{
				return D3D11ResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc(), D3D11ResourcePtr);
			}
		}
	}

	return false;
}
