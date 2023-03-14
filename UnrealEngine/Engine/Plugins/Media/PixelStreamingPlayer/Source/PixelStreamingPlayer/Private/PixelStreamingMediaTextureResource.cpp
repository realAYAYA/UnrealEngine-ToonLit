// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaTextureResource.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

FPixelStreamingMediaTextureResource::FPixelStreamingMediaTextureResource(UPixelStreamingMediaTexture* Owner)
{
	MediaTexture = Owner;
}

void FPixelStreamingMediaTextureResource::InitDynamicRHI()
{
	if (MediaTexture != nullptr)
	{
		FSamplerStateInitializerRHI SamplerStateInitializer(
			(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(MediaTexture),
			AM_Border, AM_Border, AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}
}

void FPixelStreamingMediaTextureResource::ReleaseDynamicRHI()
{
	TextureRHI.SafeRelease();

	if (MediaTexture != nullptr)
	{
		RHIUpdateTextureReference(MediaTexture->TextureReference.TextureReferenceRHI, nullptr);
	}
}

uint32 FPixelStreamingMediaTextureResource::GetSizeX() const
{
	return TextureRHI.IsValid() ? TextureRHI->GetSizeXYZ().X : 0;
}

uint32 FPixelStreamingMediaTextureResource::GetSizeY() const
{
	return TextureRHI.IsValid() ? TextureRHI->GetSizeXYZ().Y : 0;
}

SIZE_T FPixelStreamingMediaTextureResource::GetResourceSize()
{
	return CalcTextureSize(GetSizeX(), GetSizeY(), EPixelFormat::PF_A8R8G8B8, 1);
}
