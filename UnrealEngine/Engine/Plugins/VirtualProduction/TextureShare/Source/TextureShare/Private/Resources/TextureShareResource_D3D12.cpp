// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"
#include "Module/TextureShareLog.h"
#include "Core/TextureShareCoreHelpers.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"
#include "ITextureShareCoreD3D12ResourcesCache.h"

#include "ID3D12DynamicRHI.h"

namespace UE::TextureShare::Resource_D3D12
{
	// This function latter may be public in D3D12RHI
	inline DXGI_FORMAT FindSharedResourceDXGIFormat(DXGI_FORMAT InFormat, bool bSRGB)
	{
		if (bSRGB)
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:    return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case DXGI_FORMAT_BC1_TYPELESS:         return DXGI_FORMAT_BC1_UNORM_SRGB;
			case DXGI_FORMAT_BC2_TYPELESS:         return DXGI_FORMAT_BC2_UNORM_SRGB;
			case DXGI_FORMAT_BC3_TYPELESS:         return DXGI_FORMAT_BC3_UNORM_SRGB;
			case DXGI_FORMAT_BC7_TYPELESS:         return DXGI_FORMAT_BC7_UNORM_SRGB;
			};
		}
		else
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:    return DXGI_FORMAT_B8G8R8X8_UNORM;
			case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
			case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
			case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
			case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;
			};
		}
		switch (InFormat)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_UINT;
		case DXGI_FORMAT_R32G32B32_TYPELESS:    return DXGI_FORMAT_R32G32B32_UINT;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case DXGI_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_UINT;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
		case DXGI_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_UNORM;
		case DXGI_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_UNORM;
		case DXGI_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_UNORM;

		case DXGI_FORMAT_BC4_TYPELESS:         return DXGI_FORMAT_BC4_UNORM;
		case DXGI_FORMAT_BC5_TYPELESS:         return DXGI_FORMAT_BC5_UNORM;



		case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
			// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		}
		return InFormat;
	}

	static TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> GetD3D12ResourcesCache()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPI = ITextureShareCore::Get().GetTextureShareCoreAPI();

		return TextureShareCoreAPI.GetD3D12ResourcesCache();
	}
};
using namespace UE::TextureShare::Resource_D3D12;
using namespace UE::TextureShareCore;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResource::InitDynamicRHI_D3D12(FTexture2DRHIRef& OutTextureRHI)
{
	switch (ResourceDesc.ResourceType)
	{
	case ETextureShareResourceType::CrossAdapter:
		{
			// Create cross-adapter shared resource
			TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> D3D12ResourcesCache = GetD3D12ResourcesCache();
			if (D3D12ResourcesCache.IsValid())
			{
				if (ID3D12Device* D3D12DevicePtr = GetID3D12DynamicRHI()->RHIGetDevice(0))
				{
					const DXGI_FORMAT PlatformResourceFormat = FindSharedResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[ResourceSettings.Format].PlatformFormat, ResourceSettings.bShouldUseSRGB);
					FTextureShareCoreResourceHandle ResourceHandle;
					if (ID3D12Resource* D3D12CrossAdapterResource = D3D12ResourcesCache->CreateCrossAdapterResource(CoreObject->GetObjectDesc_RenderThread(), D3D12DevicePtr, GetSizeX(), GetSizeY(), PlatformResourceFormat, ResourceDesc, ResourceHandle))
					{
						OutTextureRHI = GetID3D12DynamicRHI()->RHICreateTexture2DFromResource(ResourceSettings.Format, TexCreate_Dynamic, FClearValueBinding::None, D3D12CrossAdapterResource);

						return;
					}
				}
			}
		}
		break;

	default:
		InitDynamicRHI_Default(OutTextureRHI);
		break;
	}
}

bool FTextureShareResource::D3D12RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	if (IsRHID3D12() && TextureRHI.IsValid())
	{
		ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
		ID3D12Device* D3D12DevicePtr = D3D12RHI->RHIGetDevice(0);
		ID3D12Resource* D3D12ResourcePtr = (ID3D12Resource*)D3D12RHI->RHIGetResource(TextureRHI);

		if (D3D12DevicePtr && D3D12ResourcePtr)
		{
			TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> D3D12ResourcesCache = GetD3D12ResourcesCache();
			if (D3D12ResourcesCache.IsValid())
			{
				bool bResourceHandleValid = false;
				FTextureShareCoreResourceHandle ResourceHandle;
				switch (ResourceDesc.ResourceType)
				{
					case ETextureShareResourceType::CrossAdapter:
					{
						const DXGI_FORMAT PlatformResourceFormat = FindSharedResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[ResourceSettings.Format].PlatformFormat, ResourceSettings.bShouldUseSRGB);
						bResourceHandleValid = D3D12ResourcesCache->CreateCrossAdapterResource(CoreObject->GetObjectDesc_RenderThread(), D3D12DevicePtr, GetSizeX(), GetSizeY(), PlatformResourceFormat, ResourceDesc, ResourceHandle) != nullptr;
						break;
					}

					case ETextureShareResourceType::Default:
					{
						const uint32 GPUIndex = InResourceRequest.GPUIndex;
						bResourceHandleValid = D3D12ResourcesCache->CreateSharedResource(CoreObject->GetObjectDesc_RenderThread(), D3D12DevicePtr, D3D12ResourcePtr, GPUIndex, ResourceDesc, ResourceHandle);
						break;
					}

					default:
						break;
				}

				if (bResourceHandleValid)
				{
					CoreObject->GetProxyData_RenderThread().ResourceHandles.Add(ResourceHandle);

					return true;
				}
			}
		}
	}

	return false;
}

bool FTextureShareResource::D3D12ReleaseTextureShareHandle_RenderThread()
{
	if (IsRHID3D12() && TextureRHI.IsValid())
	{
		ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
		ID3D12Device* D3D12DevicePtr = D3D12RHI->RHIGetDevice(0);
		ID3D12Resource* D3D12ResourcePtr = (ID3D12Resource*)D3D12RHI->RHIGetResource(TextureRHI);

		if (D3D12DevicePtr && D3D12ResourcePtr)
		{
			TSharedPtr<ITextureShareCoreD3D12ResourcesCache, ESPMode::ThreadSafe> D3D12ResourcesCache = GetD3D12ResourcesCache();
			if (D3D12ResourcesCache.IsValid())
			{
				return D3D12ResourcesCache->RemoveSharedResource(CoreObject->GetObjectDesc_RenderThread(), D3D12ResourcePtr);
			}
		}
	}

	return false;
}
