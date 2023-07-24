// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterViewport_Enums.h"

/**
 * nDisplay viewport render settings.
 * These are runtime settings, updated every frame from the cluster configuration.
 */
class FDisplayClusterViewport_RenderSettings
{
public:
	// Assigned camera. If empty, the currently active camera must be used
	FString CameraId;

	// Location and size on a backbuffer.
	FIntRect Rect;

public:
	// Enable this viewport and related resources rendering
	bool bEnable = true;

	// This viewport visible on final frame texture (backbuffer)
	bool bVisible = true;

	// Skip rendering for this viewport
	bool bSkipRendering = false;

	// Freeze viewport resources, skip rendering internal viewport resources. But still use it for final compositing
	bool bFreezeRendering = false;

	// This flag means no scene rendering required, but all internal resources should still be valid for
	// the media subsystem. It's a temporary solution. The flags 'bSkipRendering' and 'bFreezeRendering'
	// above, plus this one, need to be refactored at some point.
	bool bSkipSceneRenderingButLeaveResourcesAvailable = false;

	// Render alpha channel from input texture to warp output
	bool bWarpBlendRenderAlphaChannel = false;

	// Disable CustomFrustum feature from viewport settings
	bool bDisableCustomFrustumFeature = false;

	// Disable viewport overscan feature from settings
	bool bDisableFrustumOverscanFeature = false;

	// Read viewport pixels for preview (this flag is cleared at the end of the frame)
	bool bPreviewReadPixels = false;

	// Useful to render some viewports in mono, then copied to stereo backbuffers identical image
	bool bForceMono = false;

	// Is this viewport being captured by a media capture device?
	bool bIsBeingCaptured = false;

	/** Enable cross-GPU transfer for this viewport.
	  * It may be disabled in some configurations. For example, when using offscreen rendering with TextureShare,
	  * cross-gpu transfer can be disabled for this viewport to improve performance, because when transfer is called,
	  * it freezes the GPUs until synchronization is reached.
	  * (TextureShare uses its own implementation of the crossGPU transfer for the shared textures.)
	  */
	bool bEnableCrossGPUTransfer = true;

	// Performance, Multi-GPU: Asign GPU for viewport rendering. The Value '-1' used to default gpu mapping
	int32 GPUIndex = -1;

	// Performance, Multi-GPU: Customize GPU for stereo mode second view (EYE_RIGHT)
	int32 StereoGPUIndex = -1;

	// Allow ScreenPercentage 
	float BufferRatio = 1;

	// Performance: Render target base resolution multiplier
	float RenderTargetRatio = 1;

	// Performance: Render target adaptive resolution multiplier
	float RenderTargetAdaptRatio = 1;

	// Viewport can overlap each other on backbuffer. This value uses to sorting order
	int32 OverlapOrder = 0;

	// Performance: Support special frame builder mode - merge viewports to single viewfamily by group num
	// [not implemented yet] Experimental
	int32 RenderFamilyGroup = -1;

	// Special capture modes (chromakey, lightcard) change RTT format and render flags
	EDisplayClusterViewportCaptureMode CaptureMode = EDisplayClusterViewportCaptureMode::Default;

	// Override resources from this viewport
	EDisplayClusterViewportOverrideMode ViewportOverrideMode = EDisplayClusterViewportOverrideMode::None;
	FString ViewportOverrideId;

public:
	// Reset runtime values from prev frame
	inline void BeginUpdateSettings()
	{
		bVisible = true;
		bEnable = true;
		bSkipRendering = false;
		bFreezeRendering = false;
		bWarpBlendRenderAlphaChannel = false;

		CaptureMode = EDisplayClusterViewportCaptureMode::Default;

		ViewportOverrideMode = EDisplayClusterViewportOverrideMode::None;
		ViewportOverrideId.Empty();
	}

	inline void FinishUpdateSettings()
	{
		bPreviewReadPixels = false;
	}

	inline const FString& GetParentViewportId() const
	{
		return ParentViewportId;
	}

	inline bool IsViewportHasParent() const
	{
		return !ParentViewportId.IsEmpty();
	}

	inline void SetViewportOverride(const FString& InViewportOverrideId, const EDisplayClusterViewportOverrideMode InViewportOverrideMode = EDisplayClusterViewportOverrideMode::All)
	{
		ViewportOverrideMode = InViewportOverrideMode;
		ViewportOverrideId = InViewportOverrideId;
	}

	inline bool IsViewportOverrided() const
	{
		return ViewportOverrideMode != EDisplayClusterViewportOverrideMode::None && !ViewportOverrideId.IsEmpty();
	}

	// Call this after UpdateSettings()
	inline void AssignParentViewport(const FString& InParentViewportId, const FDisplayClusterViewport_RenderSettings& InParentSettings, bool Inherit = true)
	{
		ParentViewportId = InParentViewportId;

		// Inherit values from parent viewport:
		if (Inherit)
		{
			CameraId = InParentSettings.CameraId;
			Rect = InParentSettings.Rect;

			bForceMono = InParentSettings.bForceMono;

			GPUIndex = (GPUIndex < 0) ? InParentSettings.GPUIndex : GPUIndex;
			StereoGPUIndex = (StereoGPUIndex < 0) ? InParentSettings.StereoGPUIndex : StereoGPUIndex;

			RenderFamilyGroup = (RenderFamilyGroup < 0) ? InParentSettings.RenderFamilyGroup : RenderFamilyGroup;
		}
	}

protected:
	// Parent viewport name
	FString ParentViewportId;
};

