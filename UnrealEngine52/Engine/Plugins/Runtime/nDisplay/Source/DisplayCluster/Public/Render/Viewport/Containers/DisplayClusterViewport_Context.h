// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StereoRendering.h"
#include "ShowFlags.h"

class FDisplayClusterViewport_Context
{
public:
	FDisplayClusterViewport_Context(const uint32 InContextNum, const EStereoscopicPass InStereoscopicPass, const int32 InStereoViewIndex)
		: ContextNum(InContextNum)
		, StereoscopicPass(InStereoscopicPass)
		, StereoViewIndex(InStereoViewIndex)
	{}

public:
	const uint32            ContextNum;

	EStereoscopicPass StereoscopicPass;
	int32             StereoViewIndex;

	// Camera location and orientation
	FVector  ViewLocation = FVector::ZeroVector;
	bool bIsValidViewLocation = false;

	FRotator ViewRotation = FRotator::ZeroRotator;
	bool bIsValidViewRotation = false;

	// Projection Matrix
	FMatrix ProjectionMatrix = FMatrix::Identity;
	bool bIsValidProjectionMatrix = false;

	// Overscan Projection Matrix (internal use)
	FMatrix OverscanProjectionMatrix = FMatrix::Identity;

	// World scale
	float WorldToMeters = 100.f;

	//////////////////
	// Rendering data, for internal usage

	// GPU index for this context render target
	int32 GPUIndex = INDEX_NONE;

	/* Enables nDisplay's native implementation of cross-GPU transfer.
	 * This disables cross-GPU transfer by default for nDisplay viewports in FSceneViewFamily structure. **/
	bool bOverrideCrossGPUTransfer = false;

	// Location and size on a render target texture
	FIntRect RenderTargetRect;

	// Context size
	FIntPoint ContextSize;

	// Location and size on a frame target texture
	FIntRect FrameTargetRect;

	// Buffer ratio
	float CustomBufferRatio = 1;

	// Mips number for additional MipsShader resources
	int32 NumMips = 1;

	// Disable render for this viewport (Overlay)
	bool bDisableRender = false;

	struct FRenderThreadData
	{
		FRenderThreadData()
			: EngineShowFlags(ESFIM_All0)
		{ }
		
		// GPUIndex used to render this context.
		int32 GPUIndex = INDEX_NONE;

		// Display gamma used to render this context
		float EngineDisplayGamma = 2.2f;

		// Engine flags used to render this context
		FEngineShowFlags EngineShowFlags;
	};

	// This data updated only on rendering thread
	FRenderThreadData RenderThreadData;
};
