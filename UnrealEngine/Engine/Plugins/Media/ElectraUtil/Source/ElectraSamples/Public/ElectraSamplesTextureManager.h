// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHIResources.h"
#include "MediaVideoDecoderOutput.h"

class FElectraSamplesTextureManager
{
public:
	FElectraSamplesTextureManager();
	virtual ~FElectraSamplesTextureManager();

	TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> CreateTexture(const FIntPoint& Dim, EPixelFormat Fmt = PF_Unknown);
	FTextureRHIRef GetRHITextureFromPlatformTexture(void* PlatformTexture);

protected:
	virtual TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> PlatformCreateTexture(const FIntPoint& Dim, EPixelFormat Fmt) = 0;
	virtual FTextureRHIRef PlatformGetTextureRHI(IVideoDecoderTexture *Texture) = 0;
	virtual void* PlatformGetTexturePlatform(IVideoDecoderTexture* Texture) = 0;
	
	void PlatformAddSharedTextureRef(TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> InTextureRef);
	void PlatformRemoveSharedTextureRef(TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> InTextureRef);

private:
	void CleanupMap();

	FCriticalSection AccessCS;
	TMap<void*, TWeakPtr<IVideoDecoderTexture, ESPMode::ThreadSafe>> KnownTextures;
};
