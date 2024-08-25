// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationProxy.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "EngineUtils.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "UnrealClient.h"

#include "DisplayClusterSceneViewExtensions.h"

#include "LegacyScreenPercentageDriver.h"

#include "Misc/CommandLine.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/Parse.h"


///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewport::ResetRuntimeParameters()
{
	// Reset runtim flags from prev frame:
	RenderSettings.BeginUpdateSettings();
	RenderSettingsICVFX.BeginUpdateSettings();
	PostRenderSettings.BeginUpdateSettings();
	VisibilitySettings.BeginUpdateSettings();
	CameraMotionBlur.BeginUpdateSettings();
	CameraDepthOfField.BeginUpdateSettings();

	OverscanRuntimeSettings = FDisplayClusterViewport_OverscanRuntimeSettings();
	CustomFrustumRuntimeSettings = FDisplayClusterViewport_CustomFrustumRuntimeSettings();

	// Obtain viewport media state from external multicast delegates (This viewport can be used by multiple media).
	EDisplayClusterViewportMediaState AllMediaStates = EDisplayClusterViewportMediaState::None;
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().Broadcast(this, AllMediaStates);
	// Update the media state for the new frame.
	RenderSettings.AssignMediaStates(AllMediaStates);
}

bool FDisplayClusterViewport::IsInternalViewport() const
{
	// Ignore ICVFX internal resources.
	if (EnumHasAnyFlags(GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
	{
		return true;
	}

	// Ignore internal tile viewports.
	switch (GetRenderSettings().TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::None:
	case EDisplayClusterViewportTileType::Source:
		break;

	default:
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::IsExternalRendering() const
{
	if (PostRenderSettings.Replace.IsEnabled())
	{
		// The viewport is replaced by an external texture.
		return true;
	}

	if (RenderSettings.IsViewportOverridden())
	{
		// Viewport texture is overridden from another viewport.
		return true;
	}

	// UV LightCard viewport use unique whole-cluster texture from LC manager
	if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Use external texture from LightCardManager instead of rendering
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::IsRenderEnabled() const
{
	if (IsExternalRendering())
	{
		// The viewport uses an external rendering that overrides its RTT.
		// Disable rendering for this viewport.
		return false;
	}

	if (RenderSettings.bSkipRendering)
	{
		// Skip rendering.
		// For example this feature is used when the ICVFX camera uses full-frame chromakey colour,
		// thereby eliminating the ICVFX camera's viewport rendering for optimization.
		return false;
	}

	// Handle tile rendering rules.
	switch(RenderSettings.TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::Tile:
	case EDisplayClusterViewportTileType::None:
		// When tile rendering is used, only tiles are rendered.
		break;

	default:
		// When using tile rendering, other viewport types should never be rendered.
		return false;
	}

	// Handle media rendering rules.
	if (!IsRenderEnabledByMedia())
	{
		// rendering of this viewport is not allowed by media.
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::IsUsedByMedia() const
{
	return RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Input | EDisplayClusterViewportMediaState::Capture);
}

bool FDisplayClusterViewport::IsRenderEnabledByMedia() const
{
	// Use this function to disable rendering of this viewport.
	// All logic for handling media states should reside in this and IsUsedByMedia() functions only.

	static const bool bIsCluster = (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);
	static const bool bIsOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));

	const bool bHasInputAssigned = RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Input);
	const bool bHasOutputAssigned = RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Capture);

	// Do not render the viewport if it is used as a media input.
	if (bHasInputAssigned)
	{
		// This viewport is not rendered but gets the image from the media.
		// (media input replaces rendering.)
		return false;
	}

	// Applying special media rules for tiles for cluster modes only.
	if (bIsCluster && !bHasOutputAssigned
		&& RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
	{
		// Tiles logic is a little more complicated
		//
		// In | Out | Render (offscreen) | Render (onscreen)
		// ------------------------------------------------------------------
		//  0 |  0  |        0           | bShouldRenderUnbound
		//  0 |  1  |        1           | 1
		//  1 |  0  |        0           | 0
		//  1 |  1  |        0           | 0
		// Check if current node was explicitly allowed to render unbound tiles
		const bool bShouldRenderUnbound = RenderSettings.TileSettings.HasAnyTileFlags(EDisplayClusterViewportTileFlags::AllowUnboundRender);
		const bool bShouldRender = !bIsOffscreen && bShouldRenderUnbound;
		if (!bShouldRender)
		{
			// Disable tile viewport rendering.
			return false;
		}
	}

	return true;
}

bool FDisplayClusterViewport::CanSplitIntoTiles() const
{
	// Ignore internal tile viewports.
	switch(RenderSettings.TileSettings.GetType())
	{
		case EDisplayClusterViewportTileType::Tile:
		case EDisplayClusterViewportTileType::UnusedTile:
			return false;

		default:
			break;
	}

	// Ignore viewports that have a link to another.
	if (RenderSettings.IsViewportHasParent())
	{
		return false;
	}

	// Ignore viewports that use an external source instead of rendering.
	if (IsExternalRendering())
	{
		return false;
	}

	// Ignore viewports that used by media
	if (IsUsedByMedia())
	{
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::ShouldUseRenderTargetResource() const
{
	if (RenderSettings.bSkipRendering)
	{
		// When rendering is skipped, the RTT resources are not used.
		// For example this feature is used when the ICVFX camera uses full-frame chromakey colour,
		// thereby eliminating the ICVFX camera's viewport rendering resources for optimization.
		return false;
	}

	if (IsExternalRendering())
	{
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::ShouldUseInternalResources() const
{
	if (RenderSettings.bSkipRendering)
	{
		// When rendering is skipped, internal resources are not used.
		// For example this feature is used when the ICVFX camera uses full-frame chromakey colour,
		// thereby eliminating the ICVFX camera's viewport rendering resources for optimization.
		return false;
	}

	if (RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
	{
		// The tiled viewport does not use internal resources.
		// Since rendering results are copied between RTTs.
		return false;
	}


	if (RenderSettings.IsViewportOverridden())
	{
		switch (RenderSettings.GetViewportOverrideMode())
		{
		case EDisplayClusterViewportOverrideMode::All:
			// When all resources in a viewport are overridden from another viewport.
			return false;

		default:
			break;
		}
	}

	return true;
}

bool FDisplayClusterViewport::ShouldUseAdditionalTargetableResource() const
{
	check(IsInGameThread());

	// PostRender Blur require additional RTT for shader
	if (PostRenderSettings.PostprocessBlur.IsEnabled())
	{
		return true;
	}

	// Supoport projection policy additional resource
	if (ProjectionPolicy.IsValid() && ProjectionPolicy->ShouldUseAdditionalTargetableResource())
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::ShouldUseOutputTargetableResources() const
{
	// Do not create output RTTs for internal viewport
	if (IsInternalViewport())
	{
		return false;
	}

	// Only if this viewport is enabled and visible on the final frame.
	return RenderSettings.bEnable && RenderSettings.bVisible;
}

bool FDisplayClusterViewport::ShouldUseAdditionalFrameTargetableResource() const
{
	if (ShouldUseOutputTargetableResources())
	{
		// OutputFrameTargetableResources must be used for AdditionalFrameTargetableResource
		if (ViewportRemap.IsUsed())
		{
			return true;
		}
	}

	return false;
}

bool FDisplayClusterViewport::ShouldUseFullSizeFrameTargetableResource() const
{
	if (ViewportRemap.IsUsed())
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::IsOpenColorIOEquals(const FDisplayClusterViewport& InViewport) const
{
	bool bEnabledOCIO_1 = OpenColorIO.IsValid();
	bool bEnabledOCIO_2 = InViewport.OpenColorIO.IsValid();

	if (bEnabledOCIO_1 == bEnabledOCIO_2)
	{
		if (!bEnabledOCIO_1)
		{
			// Both OCIO disabled
			return true;
		}

		if (OpenColorIO->IsConversionSettingsEqual(InViewport.OpenColorIO->GetConversionSettings()))
		{
			return true;
		}
	}

	return false;
}

/**
* The viewport priority values
* A lower priority value for a viewport means that this viewport will be the first in the list of viewports.
* The order in this list is used to process viewports one after the other.
* This is important when the viewports are linked to each other.
*/
enum class EDisplayClusterViewportPriority : uint8
{
	None = 0,

	// This viewport does not use tile rendering.
	TileDisable = (1 << 0),

	// This tile source viewport should be configured before tiles.
	TileSource = (1 << 0),

	// The tile viewport setup after the tile.
	Tile = (1 << 1),

	// The linked viewport should be right after the parent viewports because it uses data from them.
	Linked = (1 << 2),

	// Overridden viewports are not rendered but depend on their source anyway, so they are processed at the end.
	Overriden = (1 << 3),
};

uint8 FDisplayClusterViewport::GetPriority() const
{
	uint8 OutOrder = 0;

	// Tile rendering requires a special viewport processing order for the game thread.
	switch (RenderSettings.TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::Source:
		OutOrder += (uint8)EDisplayClusterViewportPriority::TileSource;
		break;
	case EDisplayClusterViewportTileType::Tile:
	case EDisplayClusterViewportTileType::UnusedTile:
		OutOrder += (uint8)EDisplayClusterViewportPriority::Tile;
		break;

	case EDisplayClusterViewportTileType::None:
		OutOrder += (uint8)EDisplayClusterViewportPriority::TileDisable;
		break;

	default:
		break;
	}

	if (RenderSettings.IsViewportHasParent())
	{
		OutOrder += (uint8)EDisplayClusterViewportPriority::Linked;
	}

	if (RenderSettings.IsViewportOverridden())
	{
		OutOrder += (uint8)EDisplayClusterViewportPriority::Overriden;
	}

	return OutOrder;
}
