// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationViewport_ICVFX
///////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterShaderParametersICVFX_LightCardRenderMode FDisplayClusterConfigurationViewport_ICVFX::GetLightCardRenderMode(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	// Note: please check if this rule is valid
	if (!bAllowICVFX || !InStageSettings.Lightcard.bEnable)
	{
		// When ICVFX is disabled we dont render lightcards
		return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::None;
	}

	if (LightcardRenderMode != EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Default)
	{
		// Use overridden values from the viewport:
		switch (LightcardRenderMode)
		{
		case EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Over:
			return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::Over;

		case EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Under:
			return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::Under;

		default:
			break;
		}

		return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::None;
	}

	// Use global lightcard settings:
	switch (InStageSettings.Lightcard.Blendingmode)
	{
	case EDisplayClusterConfigurationICVFX_LightcardRenderMode::Under:
		return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::Under;

	default:
		break;
	};

	// By default lightcard render in 'Over' mode
	return EDisplayClusterShaderParametersICVFX_LightCardRenderMode::Over;
}

EDisplayClusterViewportICVFXFlags FDisplayClusterConfigurationViewport_ICVFX::GetViewportICVFXFlags(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	EDisplayClusterViewportICVFXFlags OutFlags = EDisplayClusterViewportICVFXFlags::None;
	if (bAllowICVFX)
	{
		EnumAddFlags(OutFlags, EDisplayClusterViewportICVFXFlags::Enable);
	}

	// Override camera render mode
	EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode UsedCameraRenderMode = CameraRenderMode;
	if (!bAllowInnerFrustum || !InStageSettings.bEnableInnerFrustums)
	{
		UsedCameraRenderMode = EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::Disabled;
	}

	switch (UsedCameraRenderMode)
	{
	// Disable camera frame render for this viewport
	case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::Disabled:
		EnumAddFlags(OutFlags, EDisplayClusterViewportICVFXFlags::DisableCamera | EDisplayClusterViewportICVFXFlags::DisableChromakey | EDisplayClusterViewportICVFXFlags::DisableChromakeyMarkers);
		break;

	// Disable chromakey render for this viewport
	case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::DisableChromakey:
		EnumAddFlags(OutFlags, EDisplayClusterViewportICVFXFlags::DisableChromakey | EDisplayClusterViewportICVFXFlags::DisableChromakeyMarkers);
		break;

	// Disable chromakey markers render for this viewport
	case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::DisableChromakeyMarkers:
		EnumAddFlags(OutFlags, EDisplayClusterViewportICVFXFlags::DisableChromakeyMarkers);
		break;

	default:
		break;
	}

	// Disable lightcards rendering
	const EDisplayClusterShaderParametersICVFX_LightCardRenderMode LightCardRenderMode = GetLightCardRenderMode(InStageSettings);
	if (LightCardRenderMode == EDisplayClusterShaderParametersICVFX_LightCardRenderMode::None)
	{
		EnumAddFlags(OutFlags, EDisplayClusterViewportICVFXFlags::DisableLightcard);
	}

	return OutFlags;
}
