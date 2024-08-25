// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Tile.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "DisplayClusterEnums.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_Media.h"
#include "DisplayClusterConfigurationTypes_Tile.h"
#include "DisplayClusterProjectionStrings.h"

////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationHelpers_Tile
////////////////////////////////////////////////////////////////////////

void FDisplayClusterViewportConfigurationHelpers_Tile::UpdateICVFXCameraViewportTileSettings(FDisplayClusterViewport& InSourceViewport, const FDisplayClusterConfigurationMediaICVFX& InCameraMediaSettings)
{
	// Nothing to do if media tiling is not configured
	const bool bMediaEnabled = InCameraMediaSettings.bEnable;
	const bool bMediaTiled = (InCameraMediaSettings.SplitType == EDisplayClusterConfigurationMediaSplitType::UniformTiles);
	if (!bMediaEnabled || !bMediaTiled)
	{
		InSourceViewport.GetRenderSettingsImpl().TileSettings = { };
		return;
	}

	// Validate the layout
	const bool bIsLayoutValid = FDisplayClusterConfigurationTile_Settings::IsValid(InCameraMediaSettings.TiledSplitLayout);
	if (!bIsLayoutValid)
	{
		InSourceViewport.GetRenderSettingsImpl().TileSettings = { };
		return;
	}

	// Check if viewport is able to split
	const bool bCanSplitIntoTiles = InSourceViewport.CanSplitIntoTiles();
	if (!bCanSplitIntoTiles)
	{
		InSourceViewport.GetRenderSettingsImpl().TileSettings = { };
		return;
	}

	// Find if this cluster node is allowed to render unbound tiles
	const FString ThisNodeId = InSourceViewport.GetClusterNodeId();
	const bool bAllowRenderUnbound = InCameraMediaSettings.ClusterNodesToRenderUnboundTiles.ItemNames.ContainsByPredicate([&ThisNodeId](const FString& Item)
		{
			return ThisNodeId.Equals(Item, ESearchCase::IgnoreCase);
		});

	// Generate flags
	EDisplayClusterViewportTileFlags TileFlags = EDisplayClusterViewportTileFlags::None;
	if (bAllowRenderUnbound)
	{
		TileFlags |= EDisplayClusterViewportTileFlags::AllowUnboundRender;
	}

	// Generate overscan settings
	const FDisplayClusterViewport_OverscanSettings& OverscanSettings = FDisplayClusterViewportConfigurationHelpers_Tile::GetTileOverscanSettings(InCameraMediaSettings.TileOverscan);

	// Set this viewport as a source for tile rendering.
	FDisplayClusterViewport_TileSettings& OutTileSettings = InSourceViewport.GetRenderSettingsImpl().TileSettings;
	OutTileSettings = FDisplayClusterViewport_TileSettings(InCameraMediaSettings.TiledSplitLayout, OverscanSettings, TileFlags);
	OutTileSettings.bOptimizeTileOverscan = InCameraMediaSettings.TileOverscan.bOptimizeTileOverscan;
}

FIntRect FDisplayClusterViewportConfigurationHelpers_Tile::GetDestRect(const FDisplayClusterViewport_TileSettings& InTileSettings, const FIntRect& InSourceRect)
{
	check(InTileSettings.GetType() == EDisplayClusterViewportTileType::Tile);

	const FIntPoint SourceSize(InSourceRect.Width(), InSourceRect.Height());
	const FIntPoint TileSize(FMath::RoundToZero(float(SourceSize.X) / InTileSettings.GetSize().X), FMath::RoundToZero(float(SourceSize.Y) / InTileSettings.GetSize().Y));
	const FIntPoint TilePos(InTileSettings.GetPos().X * TileSize.X, InTileSettings.GetPos().Y * TileSize.Y);

	// Dest rect min value:
	const FIntPoint DestPos = InSourceRect.Min + TilePos;

	// Dest rect size
	FIntPoint DestSize(TileSize);

	// The source size may not be divisible, and some pixels may be lost; then they will need to be restored to the edge tiles.
	if ((InTileSettings.GetPos().X + 1) == InTileSettings.GetSize().X)
	{
		DestSize.X = SourceSize.X - TilePos.X;
	}
	if ((InTileSettings.GetPos().Y + 1) == InTileSettings.GetSize().Y)
	{
		DestSize.Y = SourceSize.Y - TilePos.Y;
	}

	return FIntRect(DestPos, DestPos + DestSize);
}

FString FDisplayClusterViewportConfigurationHelpers_Tile::GetUniqueViewportNameForTile(const FString& InViewportId, const FIntPoint& TilePos)
{
	check(!InViewportId.IsEmpty());

	return FString::Printf(TEXT("%s_%s_%d_%d"), *InViewportId, DisplayClusterViewportStrings::tile::prefix, TilePos.X, TilePos.Y);
}

bool FDisplayClusterViewportConfigurationHelpers_Tile::CreateProjectionPolicyForTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& TilePos, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	FDisplayClusterConfigurationProjection CameraProjectionPolicyConfig;

	// Projection policy with type 'link' has support for tile rendering
	CameraProjectionPolicyConfig.Type = DisplayClusterProjectionStrings::projection::Link;

	// Create projection policy for viewport
	OutProjPolicy = FDisplayClusterViewportManager::CreateProjectionPolicy(GetUniqueViewportNameForTile(InSourceViewport.GetId(), TilePos), &CameraProjectionPolicyConfig);

	if (!OutProjPolicy.IsValid())
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("Tile Viewport '%s': projection policy for tile [%d-%d] not created for node '%s'."), *InSourceViewport.GetId(), TilePos.X, TilePos.Y, *InSourceViewport.GetClusterNodeId());

		return false;
	}

	return true;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_Tile::FindTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& TilePos)
{
	if (FDisplayClusterViewportManager* ViewportManager = InSourceViewport.Configuration->GetViewportManagerImpl())
	{
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> Viewport = ViewportManager->ImplFindViewport(GetUniqueViewportNameForTile(InSourceViewport.GetId(), TilePos));

		return Viewport.Get();
	}

	return nullptr;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_Tile::GetOrCreateTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& InTilePos)
{
	const FDisplayClusterViewport_RenderSettings& SourceRenderSettings = InSourceViewport.GetRenderSettings();
	if (SourceRenderSettings.TileSettings.GetType() != EDisplayClusterViewportTileType::Source)
	{
		return nullptr;
	}

	const FIntPoint& InTileSize = SourceRenderSettings.TileSettings.GetSize();

	FDisplayClusterViewport* TileViewport = FindTileViewport(InSourceViewport, InTilePos);
	if (TileViewport == nullptr)
	{
		if (FDisplayClusterViewportManager* ViewportManager = InSourceViewport.Configuration->GetViewportManagerImpl())
		{
			// Create new camera viewport
			TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> TileProjectionPolicy;
			if (CreateProjectionPolicyForTileViewport(InSourceViewport, InTilePos, TileProjectionPolicy))
			{
				// Create viewport for new projection policy
				TileViewport = ViewportManager->ImplCreateViewport(GetUniqueViewportNameForTile(InSourceViewport.GetId(), InTilePos), TileProjectionPolicy).Get();
			}
		}
	}

	// Update tile viewport settings from the source viewport
	// Note: The source viewport must already be configured.
	if (TileViewport)
	{
		// Gain direct access to internal resources of the NewViewport:
		FDisplayClusterViewport_RenderSettings& InOutRenderSettings = TileViewport->GetRenderSettingsImpl();

		// Reset runtime flags from prev frame.
		// Also this function update media states.
		TileViewport->ResetRuntimeParameters();

		// Override rendering settings from the source viewport, except for media states.
		{
			// Save the new media states after calling ResetRuntimeParameters().
			const EDisplayClusterViewportMediaState TileMediaStates = TileViewport->GetRenderSettings().GetMediaStates();

			// Copy all the settings from the source viewport, but some of them still need to be overridden.
			InOutRenderSettings = SourceRenderSettings;

			// Restore media states
			InOutRenderSettings.AssignMediaStates(TileMediaStates);
		}

		// Don't show Tile composing viewports on frame target
		InOutRenderSettings.bVisible = false;

		// Disable custom frustum settings and override overscan settings.
		// Use custom overscan settings for the tile rendering.
		InOutRenderSettings.CustomFrustumSettings = FDisplayClusterViewport_CustomFrustumSettings();
		InOutRenderSettings.OverscanSettings = SourceRenderSettings.TileSettings.GetOverscanSettings();

		// Optimize overscan values for edge tiles
		if (SourceRenderSettings.TileSettings.bOptimizeTileOverscan)
		{
			if (InTilePos.X == 0)
			{
				InOutRenderSettings.OverscanSettings.Left = 0;
			}
			if (InTilePos.Y == 0)
			{
				InOutRenderSettings.OverscanSettings.Top = 0;
			}
			if (InTilePos.X == (InTileSize.X - 1))
			{
				InOutRenderSettings.OverscanSettings.Right = 0;
			}
			if (InTilePos.Y == (InTileSize.Y - 1))
			{
				InOutRenderSettings.OverscanSettings.Bottom = 0;
			}
		}

		// Inherit tile flags from the source viewport
		const EDisplayClusterViewportTileFlags TileFlags = InSourceViewport.GetRenderSettings().TileSettings.GetTileFlags();

		// Setup as tile.
		InOutRenderSettings.TileSettings = FDisplayClusterViewport_TileSettings(InSourceViewport.GetId(), InTilePos, InTileSize, TileFlags);

		// Copy internal render settings from the source:
		TileViewport->GetCustomPostProcessSettings() = InSourceViewport.GetCustomPostProcessSettings();
		TileViewport->GetVisibilitySettingsImpl() = InSourceViewport.GetVisibilitySettingsImpl();
		TileViewport->GetCameraMotionBlurImpl() = InSourceViewport.GetCameraMotionBlurImpl();
		TileViewport->GetCameraDepthOfFieldImpl() = InSourceViewport.GetCameraDepthOfFieldImpl();

		// Copy OCIO.
		TileViewport->SetOpenColorIO(InSourceViewport.GetOpenColorIO());
	}

	return TileViewport;
}

FDisplayClusterViewport_OverscanSettings FDisplayClusterViewportConfigurationHelpers_Tile::GetTileOverscanSettings(const FDisplayClusterConfigurationTile_Overscan& InTileOverscan)
{
	FDisplayClusterViewport_OverscanSettings OutOverscanSettings;

	OutOverscanSettings.bEnabled = false;
	OutOverscanSettings.bOversize = InTileOverscan.bOversize;

	if (InTileOverscan.bEnabled)
	{
		switch (InTileOverscan.Mode)
		{
		case EDisplayClusterConfigurationViewportOverscanMode::Percent:
			OutOverscanSettings.bEnabled = InTileOverscan.bEnabled;
			OutOverscanSettings.Unit = EDisplayClusterViewport_FrustumUnit::Percent;

			// Scale 0..100% to 0..1 range
			OutOverscanSettings.Left =
			OutOverscanSettings.Right = 
			OutOverscanSettings.Top = 
			OutOverscanSettings.Bottom = .01f * InTileOverscan.AllSides;
			break;

		case EDisplayClusterConfigurationViewportOverscanMode::Pixels:
			OutOverscanSettings.bEnabled = InTileOverscan.bEnabled;
			OutOverscanSettings.Unit = EDisplayClusterViewport_FrustumUnit::Pixels;

			OutOverscanSettings.Left =
			OutOverscanSettings.Right =
			OutOverscanSettings.Top =
			OutOverscanSettings.Bottom = InTileOverscan.AllSides;
			break;

		default:
			break;
		}
	}

	return OutOverscanSettings;
}
