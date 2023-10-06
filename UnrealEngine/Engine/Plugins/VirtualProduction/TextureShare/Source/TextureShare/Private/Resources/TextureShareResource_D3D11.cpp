// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"
#include "ITextureShareCoreD3D11ResourcesCache.h"

#include "ID3D11DynamicRHI.h"

namespace UE::TextureShare::Resource_D3D11
{
	static TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> GetD3D11ResourcesCache()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPI = ITextureShareCore::Get().GetTextureShareCoreAPI();

		return TextureShareCoreAPI.GetD3D11ResourcesCache();
	}
};
using namespace UE::TextureShare::Resource_D3D11;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::D3D11RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	if (IsRHID3D11() && TextureRHI.IsValid())
	{
		TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> D3D11ResourcesCache = GetD3D11ResourcesCache();
		if (D3D11ResourcesCache.IsValid())
		{
			ID3D11DynamicRHI* D3D11RHI = GetID3D11DynamicRHI();
			ID3D11Device* D3D11DevicePtr = D3D11RHI->RHIGetDevice();
			ID3D11Texture2D* D3D11ResourcePtr = (ID3D11Texture2D*)D3D11RHI->RHIGetResource(TextureRHI);

			if (D3D11DevicePtr && D3D11ResourcePtr)
			{
				FTextureShareCoreResourceHandle ResourceHandle;
				if (D3D11ResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc_RenderThread(), D3D11DevicePtr, D3D11ResourcePtr, ResourceDesc, ResourceHandle))
				{
					CoreObject->GetProxyData_RenderThread().ResourceHandles.Add(ResourceHandle);

					return true;
				}
			}
		}
	}

	return false;
}

bool FTextureShareResource::D3D11ReleaseTextureShareHandle_RenderThread()
{
	if (IsRHID3D11() && TextureRHI.IsValid())
	{
		ID3D11DynamicRHI* D3D11RHI = GetID3D11DynamicRHI();
		if (ID3D11Texture2D* D3D11ResourcePtr = (ID3D11Texture2D*)D3D11RHI->RHIGetResource(TextureRHI))
		{
			TSharedPtr<ITextureShareCoreD3D11ResourcesCache, ESPMode::ThreadSafe> D3D11ResourcesCache = GetD3D11ResourcesCache();
			if (D3D11ResourcesCache.IsValid())
			{
				return D3D11ResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc_RenderThread(), D3D11ResourcePtr);
			}
		}
	}

	return false;
}
