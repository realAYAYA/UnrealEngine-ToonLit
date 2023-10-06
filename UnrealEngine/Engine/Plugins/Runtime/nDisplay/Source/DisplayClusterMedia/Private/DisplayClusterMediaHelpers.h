// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;
class FRHITexture;


namespace DisplayClusterMediaHelpers
{
	namespace MediaId
	{
		// Helper for media ID generation
		enum class EMediaDeviceType : uint8
		{
			Input,
			Output
		};

		// Helper for media ID generation
		enum class EMediaOwnerType : uint8
		{
			Backbuffer,
			Viewport,
			ICVFXCamera
		};

		// Generate media ID for a specific entity
		FString GenerateMediaId(EMediaDeviceType DeviceType, EMediaOwnerType OwnerType, const FString& NodeId, const FString& DCRAName, const FString& OwnerName, uint8 Index);
	}

	// Generates internal ICVFX viewport IDs
	FString GenerateICVFXViewportName(const FString& ClusterNodeId, const FString& ICVFXCameraName);

	// Copy and resize an RHI texture
	void ResampleTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect);
}
