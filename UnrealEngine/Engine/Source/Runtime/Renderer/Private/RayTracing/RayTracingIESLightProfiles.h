// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Engine/TextureLightProfile.h"
#include "RendererInterface.h"

#if RHI_RAYTRACING

class FIESLightProfileResource
{
public:
	void BuildIESLightProfilesTexture(FRHICommandListImmediate& RHICmdList, const TArray<UTextureLightProfile*, SceneRenderingAllocator>& NewIESProfilesArray);

	uint32 GetIESLightProfilesCount() const
	{
		return IESTextureData.Num();
	}

	void Release()
	{
		check(IsInRenderingThread());

		DefaultTexture.SafeRelease();
		AtlasTexture.SafeRelease();
		AtlasUAV.SafeRelease();
		IESTextureData.Empty();
	}

	FTexture2DRHIRef GetTexture()
	{
		return AtlasTexture;
	}

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

private:
	FTexture2DRHIRef					DefaultTexture;
	FTexture2DRHIRef					AtlasTexture;
	FUnorderedAccessViewRHIRef			AtlasUAV;
	TArray<const UTextureLightProfile*>	IESTextureData;

	bool IsIESTextureFormatValid(const UTextureLightProfile* Texture) const;

	static constexpr uint32 AllowedIESProfileWidth = 256;
	static constexpr EPixelFormat AllowedIESProfileFormat = PF_FloatRGBA;
};

#endif // RHI_RAYTRACING
