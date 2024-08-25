// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

/**
* Viewport preview-in-scene rendering settings.
*/
struct FDisplayClusterViewport_PreviewSettings
{
	// The IDisplayClusterViewportManagerPreview::UpdateEntireClusterPreviewRender() function will use this rendering mode
	// For special rendering cases, set a different value, such as EDisplayClusterRenderFrameMode::PreviewProxyHitInScene
	EDisplayClusterRenderFrameMode EntireClusterPreviewRenderMode = EDisplayClusterRenderFrameMode::PreviewInScene;

	// Render the scene and display it as a preview on the nDisplay root actor in the editor.  This will impact editor performance.
	bool bPreviewEnable = false;

	// Render this DCRA in game for Standalone/Package builds.
	bool bPreviewInGameEnable = false;
	bool bPreviewInGameRenderFrustum = false;

	// Preview uses techvis mode for rendering
	bool bEnablePreviewTechvis = false;

	// Enable/Disable preview rendering. When disabled preview image freeze
	bool bFreezePreviewRender = false;

	// Hack preview gamma.
	// In a scene, PostProcess always renders on top of the preview textures.
	// But in it, PostProcess is also rendered with the flag turned off.
	bool bPreviewEnablePostProcess = false;

	// Show overlay material on the preview mesh when preview rendering is enabled (UMeshComponent::OverlayMaterial)
	bool bPreviewEnableOverlayMaterial = true;

	// Allows you to process preview meshes inside DCRA (get or create a mesh from a projection policy, update materials on the preview mesh, etc.).
	bool bEnablePreviewMesh = false;

	// Allows you to process preview editable meshes inside DCRA (get or create a mesh from a projection policy, update materials on the preview editable mesh, etc.).
	bool bEnablePreviewEditableMesh = false;

	// Render ICVFX Frustums
	bool bPreviewICVFXFrustums = false;
	float PreviewICVFXFrustumsFarDistance = 1000.0f;

	// Preview RTT size multiplier
	float PreviewRenderTargetRatioMult = 1.f;

	// The maximum dimension of any texture for preview
	// Limit preview textures max size
	int32 PreviewMaxTextureDimension = 2048;

	// Tick Per Frame
	int TickPerFrame = 1;

	// Max amount of Viewports Per Frame
	int ViewportsPerFrame = 1;

	// The DisplayDevice component will be obtained from the RootActor with the specified type
	EDisplayClusterRootActorType DisplayDeviceRootActorType = EDisplayClusterRootActorType::Configuration;
};
