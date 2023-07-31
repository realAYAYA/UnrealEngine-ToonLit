// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
#include "OculusHMD.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FD3D11CustomPresent
//-------------------------------------------------------------------------------------------------

class FD3D11CustomPresent : public FCustomPresent
{
public:
	FD3D11CustomPresent(FOculusHMD* InOculusHMD);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual bool IsUsingCorrectDisplayAdapter() const override;
	virtual void* GetOvrpDevice() const override;
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, ETextureCreateFlags InTexCreateFlags) override;
};


FD3D11CustomPresent::FD3D11CustomPresent(FOculusHMD* InOculusHMD) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_D3D11, PF_B8G8R8A8, true)
{
	switch (GPixelFormats[PF_DepthStencil].PlatformFormat)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		DefaultDepthOvrpTextureFormat = ovrpTextureFormat_D24_S8;
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		DefaultDepthOvrpTextureFormat = ovrpTextureFormat_D32_S824_FP;
		break;
	default:
		UE_LOG(LogHMD, Error, TEXT("Unrecognized depth buffer format"));
		break;
	}
}


bool FD3D11CustomPresent::IsUsingCorrectDisplayAdapter() const
{
	const void* luid;

	if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetDisplayAdapterId2(&luid)) && luid)
	{
		IDXGIAdapter* Adapter = GetID3D11DynamicRHI()->RHIGetAdapter();
		DXGI_ADAPTER_DESC AdapterDesc{};

		if (Adapter && SUCCEEDED(Adapter->GetDesc(&AdapterDesc)))
		{
			return !FMemory::Memcmp(luid, &AdapterDesc.AdapterLuid, sizeof(LUID));
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}


void* FD3D11CustomPresent::GetOvrpDevice() const
{
	return GetID3D11DynamicRHI()->RHIGetDevice();
}


FTextureRHIRef FD3D11CustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, ETextureCreateFlags InTexCreateFlags)
{
	CheckInRenderThread();

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return GetID3D11DynamicRHI()->RHICreateTexture2DFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D11Texture2D*) InTexture).GetReference();

	case RRT_Texture2DArray:
		return GetID3D11DynamicRHI()->RHICreateTexture2DArrayFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D11Texture2D*)InTexture).GetReference();

	case RRT_TextureCube:
		return GetID3D11DynamicRHI()->RHICreateTextureCubeFromResource(InFormat, InTexCreateFlags | TexCreate_TargetArraySlicesIndependently, InBinding, (ID3D11Texture2D*) InTexture).GetReference();

	default:
		return nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_D3D11(FOculusHMD* InOculusHMD)
{
	return new FD3D11CustomPresent(InOculusHMD);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
