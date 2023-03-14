// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterRenderFrameEnums.h"
#include "SceneViewExtension.h"

// Render frame container
class FDisplayClusterRenderFrame
{
public:
	~FDisplayClusterRenderFrame()
	{
		RenderTargets.Empty();
	}

public:
	class FFrameView
	{
	public:
		inline bool ShouldRenderSceneView() const
		{
			return !bDisableRender && !bFreezeRendering;
		}

	public:
		// Viewport game-thread data
		class IDisplayClusterViewport* Viewport = nullptr;

		// Viewport context index for this view
		uint32 ContextNum = 0;

		bool bDisableRender = false;
		bool bFreezeRendering = false;
	};

	class FFrameViewFamily
	{
	public:
		~FFrameViewFamily()
		{
			ViewExtensions.Empty();
			Views.Empty();
		}

	public:
		// Customize ScreenPercentage feature for viewfamily
		float CustomBufferRatio = 1;

		// Extensions that can modify view parameters
		TArray<FSceneViewExtensionRef> ViewExtensions;

		// Vieports, rendered at once for tthis family
		TArray<FFrameView> Views;

		int32 NumViewsForRender = 0;
	};

	class FFrameRenderTarget
	{
	public:
		~FFrameRenderTarget()
		{
			ViewFamilies.Empty();
		}

	public:
		// Discard some RTT (when view render disabled)
		// Also when RTT Atlasing used, this viewports excluded from atlas map (reduce size)
		bool bShouldUseRenderTarget = true;

		// required Render target size (resource can be bigger)
		FIntPoint RenderTargetSize;

		EDisplayClusterViewportCaptureMode CaptureMode = EDisplayClusterViewportCaptureMode::Default;

		// Render target resource ref
		class FRenderTarget* RenderTargetPtr = nullptr;

		// Families, rendered on this target
		TArray<FFrameViewFamily> ViewFamilies;
	};

public:
	// Render frame to this targets
	TArray<FFrameRenderTarget> RenderTargets;
	
	// Frame rect on final backbuffer
	FIntRect FrameRect;

	int32 DesiredNumberOfViews = 0;
	int32 ViewportsAmount = 0;

	IDisplayClusterViewportManager* ViewportManager = nullptr;
};

