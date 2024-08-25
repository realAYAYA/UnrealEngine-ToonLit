// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettingsEnums.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PreviewSettings.h"

// Settings for render frame builder
struct FDisplayClusterRenderFrameSettings
{
	// customize mono\stereo render modes
	EDisplayClusterRenderFrameMode RenderMode = EDisplayClusterRenderFrameMode::Unknown;

	// nDisplay has its own implementation of cross-GPU transfer.
	struct FCrossGPUTransfer
	{
		// Enable cross-GPU transfers using nDisplay
		// That replaces the default cross-GPU transfers using UE Core for the nDisplay viewports viewfamilies.
		bool bEnable = false;

		// The bLockSteps parameter is simply passed to the FTransferResourceParams structure.
		// Whether the GPUs must handshake before and after the transfer. Required if the texture rect is being written to in several render passes.
		// Otherwise, minimal synchronization will be used.
		bool bLockSteps = false;

		// The bPullData parameter is simply passed to the FTransferResourceParams structure.
		// Whether the data is read by the dest GPU, or written by the src GPU (not allowed if the texture is a backbuffer)
		bool bPullData = true;

	} CrossGPUTransfer;

	// Alpha channel capture mode for viewports (Lightcard, chromakey)
	EDisplayClusterRenderFrameAlphaChannelCaptureMode AlphaChannelCaptureMode;


	// Some frame postprocess require additional render targetable resources
	bool bShouldUseAdditionalFrameTargetableResource = false;

	// Postprocess can use full size backbuffer. This disables Frame RTT size optimization
	bool bShouldUseFullSizeFrameTargetableResource = false;

	// Create output resources only for visible viewports. This will save GPU memory space.
	bool bShouldUseOutputTargetableResources = false;


	// Multiply all viewports RTT size's for whole cluster by this value
	float ClusterRenderTargetRatioMult = 1.f;

	// Multiply inner frustum RTT size's for whole cluster by this value
	float ClusterICVFXInnerViewportRenderTargetRatioMult = 1.f;

	// Multiply outer viewports RTT size's for whole cluster by this value
	float ClusterICVFXOuterViewportRenderTargetRatioMult = 1.f;


	// Multiply all buffer ratios for whole cluster by this value
	float ClusterBufferRatioMult = 1.f;

	// Multiply inner frustums buffer ratios for whole cluster by this value
	float ClusterICVFXInnerFrustumBufferRatioMult = 1.f;

	// Multiply outer viewports buffer ratios for whole cluster by this value
	float ClusterICVFXOuterViewportBufferRatioMult = 1.f;

	// Allow warpblend render
	bool bAllowWarpBlend = true;

	// Settings for preview rendering
	FDisplayClusterViewport_PreviewSettings PreviewSettings;

	// [Experimental] Render preview in multi-GPU
	// Specifies the mGPU index range for rendering the DCRA preview.
	TOptional<FIntPoint> PreviewMultiGPURendering;

	// Use DC render device for rendering
	bool bUseDisplayClusterRenderDevice = true;

	// Performance: Stereoscopic rendering uses a single viewfamily(RTT) for both eyes
	bool bEnableStereoscopicRenderingOptimization = false;

	// Performance: Allow to use parent ViewFamily from parent viewport 
	// (icvfx has child viewports: lightcard and chromakey with prj_view matrices copied from parent viewport. May sense to use same viewfamily?)
	// [not implemented yet] Experimental
	bool bShouldUseParentViewportRenderFamily = false;

	// Cluster node name for render
	FString ClusterNodeId;

public:
	/** Current frame is preview. */
	bool IsPreviewRendering() const;

	/** Returns true, if Techvis is used. */
	bool IsTechvisEnabled() const;

	/** Returns true if the DCRA preview feature in Standalone/Package builds is used. */
	bool IsPreviewInGameEnabled() const;

	/** returns true if the preview rendering has been updated.If this function returns false, the DCRA preview image should be frozen. */
	bool IsPreviewFreezeRender() const;

	/** [Experimental] Get the GPU range used for rendering preview.
	* returns nullptr if this rendering function is disabled on mGPU
	*/
	const FIntPoint* GetPreviewMultiGPURendering() const;

	/** PostProcess should be disabled for the current frame. */
	bool IsPostProcessDisabled() const;

	/** Should use linear gamma. */
	bool ShouldUseLinearGamma() const;

	/** true, if output frame RTTs is used. */
	bool ShouldUseOutputFrameTargetableResources() const;

	/** Is stereo rendering on monoscopic display (sbs, tb) . */
	bool ShouldUseStereoRenderingOnMonoscopicDisplay() const;

	/** Getting the desired frame size multipliers. */
	FVector2D GetDesiredFrameMult() const;

	/** Obtain the desired RTT size (for sbs and tb this is half the size in one of the dimensions). */
	FVector2D GetDesiredRTTSize(const FVector2D& InSize) const;
	FIntPoint GetDesiredRTTSize(const FIntPoint& InSize) const;

	/** Get the maximum texture size that is allowed to be used in the viewport. */
	int32 GetViewportTextureMaxSize() const;

	/** Should use Cross-GPU transfers. */
	inline bool ShouldUseCrossGPUTransfers() const
	{
		return bUseDisplayClusterRenderDevice || GetPreviewMultiGPURendering() != nullptr;
	}

	/** Ability to reuse the viewport across all nodes in the cluster (DCRA Preview). */
	bool CanReuseViewportWithinClusterNodes() const;

	/** Return number of contexts per viewport. */
	int32 GetViewPerViewportAmount() const;
};
