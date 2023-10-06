// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StereoRendering.h"
#include "ShowFlags.h"

/**
 * Special flags for nDisplay Viewport context
 */
enum class EDisplayClusterViewportContextState : uint8
{
	None = 0,

	// The FDisplayClusterViewport::CalculateView() function can be called several times per frame.
	// Each time it must return the same values. For optimization purposes, after the first call this function
	// stores the result in the context variables 'ViewLocation' and 'ViewRotation'.
	// Finally, raises this flag for subsequent calls in the current frame.
	HasCalculatedViewPoint = 1 << 0,

	// Viewpoint is not valid for this viewport (cannot be calculated)
	InvalidViewPoint = 1 << 1,

	// The FDisplayClusterViewport::GetProjectionMatrix() function can also be called several times per frame.
	// stores the result in the context variables 'ProjectionMatrix' and 'OverscanProjectionMatrix'.
	// Finally, raises this flag for subsequent calls in the current frame.
	HasCalculatedProjectionMatrix = 1 << 2,
	HasCalculatedOverscanProjectionMatrix = 1 << 3,

	// The projection matrix is not valid (cannot be calculated)
	InvalidProjectionMatrix = 1 << 4,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportContextState);

/**
 * Viewport context with cahched data and states
 */
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

	// Special flags for this context
	EDisplayClusterViewportContextState ContextState = EDisplayClusterViewportContextState::None;

	// ViewPoint: Camera location and orientation
	FVector  ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;

	// Projection Matrix
	FMatrix ProjectionMatrix = FMatrix::Identity;

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
