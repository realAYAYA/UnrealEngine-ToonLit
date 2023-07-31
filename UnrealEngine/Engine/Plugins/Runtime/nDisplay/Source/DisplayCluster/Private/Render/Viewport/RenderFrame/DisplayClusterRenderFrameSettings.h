// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"

// Settings for render frame builder
struct FDisplayClusterRenderFrameSettings
{
	// customize mono\stereo render modes
	EDisplayClusterRenderFrameMode RenderMode = EDisplayClusterRenderFrameMode::Mono;

	// Control mGPU for whole cluster
	EDisplayClusterMultiGPUMode MultiGPUMode = EDisplayClusterMultiGPUMode::Enabled;

	// Some frame postprocess require additional render targetable resources
	bool bShouldUseAdditionalFrameTargetableResource = false;

	// Postprocess can use full size backbuffer. This disables Frame RTT size optimization
	bool bShouldUseFullSizeFrameTargetableResource = false;

	// Enable Post Process for preview rendering
	bool bPreviewEnablePostProcess = false;

	// Preview RTT size multiplier
	float PreviewRenderTargetRatioMult = 1.f;


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

	// Render in Editor mode
	bool bIsRenderingInEditor = false;

	// Configuration used to render preview
	bool bIsPreviewRendering = false;

	// Allow mGPU in editor mode
	bool bAllowMultiGPURenderingInEditor = false;
	int32 PreviewMinGPUIndex = 0;
	int32 PreviewMaxGPUIndex = 0;

	// The maximum dimension of any texture for preview
	int32 PreviewMaxTextureDimension = 2048;

	// Performance: Stereoscopic rendering uses a single viewfamily(RTT) for both eyes
	bool bEnableStereoscopicRenderingOptimization = false;

	// Performance: Allow merge multiple viewports on single RTT with atlasing (required for bAllowViewFamilyMergeOptimization)
	bool bAllowRenderTargetAtlasing = false;

	// Performance: Allow viewfamily merge optimization (render multiple viewports contexts within single family)
	// [not implemented yet] Experimental
	EDisplayClusterRenderFamilyMode ViewFamilyMode = EDisplayClusterRenderFamilyMode::None;

	// Performance: Allow to use parent ViewFamily from parent viewport 
	// (icvfx has child viewports: lightcard and chromakey with prj_view matrices copied from parent viewport. May sense to use same viewfamily?)
	// [not implemented yet] Experimental
	bool bShouldUseParentViewportRenderFamily = false;

	// Cluster node name for render
	FString ClusterNodeId;
};


