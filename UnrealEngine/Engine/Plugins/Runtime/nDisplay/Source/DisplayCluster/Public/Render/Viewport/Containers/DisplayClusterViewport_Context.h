// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StereoRendering.h"
#include "ShowFlags.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

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
	{ }

public:
	// Index of context (eye #)
	const uint32            ContextNum;

	// References to IStereoRendering
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

	/** Cached projection data */
	struct FCachedProjectionData
	{
		bool bValid = false;

		// Is overscan used
		bool bUseOverscan = false;

		// Projection angles [Left, Right, Top, Bottom]
		FVector4 ProjectionAngles;

		// Projection angles for Overscan [Left, Right, Top, Bottom]
		FVector4 OverscanProjectionAngles;

		// Projection planes
		double ZNear = 0.f;
		double ZFar = 0.f;
	};
	
	// Cached projection values
	// This values updated from function FDisplayClusterViewport::CalculateProjectionMatrix()
	FCachedProjectionData ProjectionData;

	// World scale
	float WorldToMeters = 100.f;

	// GPU index for this context render target
	int32 GPUIndex = INDEX_NONE;

	// Enables nDisplay's native implementation of cross-GPU transfer.
	// This disables cross-GPU transfer by default for nDisplay viewports in FSceneViewFamily structure.
	bool bOverrideCrossGPUTransfer = false;

	// Location and size on a render target texture
	FIntRect RenderTargetRect;

	// Context size
	FIntPoint ContextSize;

	// Location and size on a frame target texture
	FIntRect FrameTargetRect;

	// Tile location and size in the source viewport
	FIntRect TileDestRect;

	// Buffer ratio
	float CustomBufferRatio = 1;

	// Mips number for additional MipsShader resources
	int32 NumMips = 1;

	// Disable render for this viewport (Overlay)
	bool bDisableRender = false;

	/**
	* Viewport context data for rendering thread
	*/
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
