// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;
class FRHITexture;


namespace DisplayClusterMediaHelpers
{
	// Generates internal ICVFX viewport IDs
	FString GenerateICVFXViewportName(const FString& ClusterNodeId, const FString& ICVFXCameraName);

	// Copy and resize an RHI texture
	void ResampleTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect);
}
