// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "BinkMediaTexture.h"

struct FBinkMediaTextureResource : FTextureResource, FRenderTarget, FDeferredUpdateResource 
{
	FBinkMediaTextureResource(const UBinkMediaTexture* InOwner, EPixelFormat fmt)
		: Owner(InOwner), PixelFormat(fmt)
	{ 
	}

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;
	virtual FIntPoint GetSizeXY() const override { return Owner->CachedDimensions; }
	virtual void UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget = true) override;

	void Clear();

	const UBinkMediaTexture* Owner;
	EPixelFormat PixelFormat;
};
