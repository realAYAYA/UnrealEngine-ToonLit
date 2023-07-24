// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIResources.h"

#include "WarpBlend/DisplayClusterWarpContext.h"
#include "WarpBlend/IDisplayClusterWarpBlend.h"

struct FDisplayClusterShaderParameters_WarpBlend
{
	struct FResourceWithRect
	{
		FRHITexture2D* Texture;
		FIntRect       Rect;

		void Set(FRHITexture2D* InTexture, const FIntRect& InRect)
		{
			Texture = InTexture;
			Rect = InRect;
		}
	};

	// In\Out resources for warp
	FResourceWithRect Src;
	FResourceWithRect Dest;

	// Warp interface
	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpInterface;

	// Context data
	FDisplayClusterWarpContext Context;

	// Render alpha channel from input texture to warp output
	bool bRenderAlphaChannel = false;
};
