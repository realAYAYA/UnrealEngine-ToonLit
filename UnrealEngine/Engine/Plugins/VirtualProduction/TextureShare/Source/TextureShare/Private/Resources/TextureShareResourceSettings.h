// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"

struct FTextureShareCoreResourceRequest;
class FRHITexture;

/**
 * TextureShare resource settings
 */
struct FTextureShareResourceSettings
{
	FTextureShareResourceSettings() = default;
	FTextureShareResourceSettings(const FTextureShareCoreResourceRequest& InResourceRequest, FRHITexture* InTexture);

	bool Initialize(const FTextureShareCoreResourceRequest& InResourceRequest);

	bool Equals(const FTextureShareResourceSettings& In) const;

	FIntPoint Size;
	EPixelFormat Format = PF_Unknown;

	uint32 NumMips = 1;
	bool bShouldUseSRGB = false;
};
