// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_ICVFX.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_ChromakeyMarkers
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterConfigurationICVFX_ChromakeyMarkers::FDisplayClusterConfigurationICVFX_ChromakeyMarkers()
{
	// Default marker texture
	const FString TexturePath = TEXT("/nDisplay/Textures/T_TrackingMarker_A.T_TrackingMarker_A");
	MarkerTileRGBA = Cast<UTexture2D>(FSoftObjectPath(TexturePath).TryLoad());
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_CameraRenderSettings
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterConfigurationICVFX_CameraRenderSettings::FDisplayClusterConfigurationICVFX_CameraRenderSettings()
{
	// Setup incamera defaults:
	GenerateMips.bAutoGenerateMips = true;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_LightcardCustomOCIO
///////////////////////////////////////////////////////////////////////////////////////
const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_LightcardCustomOCIO::FindOCIOConfiguration(const FString& InViewportId) const
{
	if (AllViewportsOCIOConfiguration.bIsEnabled)
	{
		// Per viewport OCIO:
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : PerViewportOCIOProfiles)
		{
			if (OCIOProfileIt.IsEnabledForObject(InViewportId))
			{
				return &OCIOProfileIt.ColorConfiguration;
			}
		}

		if (AllViewportsOCIOConfiguration.IsEnabled())
		{
			return &AllViewportsOCIOConfiguration.ColorConfiguration;
		}
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_ViewportOCIO
///////////////////////////////////////////////////////////////////////////////////////
const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_ViewportOCIO::FindOCIOConfiguration(const FString& InViewportId) const
{
	if (AllViewportsOCIOConfiguration.bIsEnabled)
	{
		// Per viewport OCIO:
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : PerViewportOCIOProfiles)
		{
			if (OCIOProfileIt.IsEnabledForObject(InViewportId))
			{
				return &OCIOProfileIt.ColorConfiguration;
			}
		}

		if (AllViewportsOCIOConfiguration.IsEnabled())
		{
			return &AllViewportsOCIOConfiguration.ColorConfiguration;
		}
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_CameraOCIO
///////////////////////////////////////////////////////////////////////////////////////
const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_CameraOCIO::FindOCIOConfiguration(const FString& InClusterNodeId) const
{
	if (AllNodesOCIOConfiguration.bIsEnabled)
	{
		// Per node OCIO:
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : PerNodeOCIOProfiles)
		{
			if (OCIOProfileIt.IsEnabledForObject(InClusterNodeId))
			{
				return &OCIOProfileIt.ColorConfiguration;
			}
		}

		if (AllNodesOCIOConfiguration.IsEnabled())
		{
			return &AllNodesOCIOConfiguration.ColorConfiguration;
		}
	}

	return nullptr;
}

#if WITH_EDITOR
bool FDisplayClusterConfigurationICVFX_CameraOCIO::IsChromakeyViewportSettingsEqual_Editor(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return IsInnerFrustumViewportSettingsEqual_Editor(InClusterNodeId1, InClusterNodeId2);
}

bool FDisplayClusterConfigurationICVFX_CameraOCIO::IsInnerFrustumViewportSettingsEqual_Editor(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	if (AllNodesOCIOConfiguration.bIsEnabled)
	{
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : PerNodeOCIOProfiles)
		{
			if (OCIOProfileIt.IsEnabled())
			{
				const FString* CustomNode1 = OCIOProfileIt.ApplyOCIOToObjects.FindByPredicate([ClusterNodeId = InClusterNodeId1](const FString& InClusterNodeId)
					{
						return ClusterNodeId.Equals(InClusterNodeId, ESearchCase::IgnoreCase);
					});

				const FString* CustomNode2 = OCIOProfileIt.ApplyOCIOToObjects.FindByPredicate([ClusterNodeId = InClusterNodeId2](const FString& InClusterNodeId)
					{
						return ClusterNodeId.Equals(InClusterNodeId, ESearchCase::IgnoreCase);
					});

				if (CustomNode1 && CustomNode2)
				{
					// equal custom settings
					return true;
				}

				if (CustomNode1 || CustomNode2)
				{
					// one of node has custom settings
					return false;
				}
			}
		}
	}

	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_LightcardOCIO
///////////////////////////////////////////////////////////////////////////////////////
const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_LightcardOCIO::FindOCIOConfiguration(const FString& InViewportId, const FDisplayClusterConfigurationICVFX_ViewportOCIO& InViewportOCIO) const
{
	switch (LightcardOCIOMode)
	{
	case EDisplayClusterConfigurationViewportLightcardOCIOMode::nDisplay:
		// Use Viewport OCIO
		return InViewportOCIO.FindOCIOConfiguration(InViewportId);

	case EDisplayClusterConfigurationViewportLightcardOCIOMode::Custom:
		// Use custom OCIO
		return CustomOCIO.FindOCIOConfiguration(InViewportId);

	default:
		// No OCIO for Light Cards
		break;
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_CameraSettings
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterConfigurationICVFX_CameraSettings::FDisplayClusterConfigurationICVFX_CameraSettings()
{
	AllNodesColorGrading.bEnableEntireClusterColorGrading = true;
}

const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_CameraSettings::FindInnerFrustumOCIOConfiguration(const FString& InClusterNodeId) const
{
	return CameraOCIO.FindOCIOConfiguration(InClusterNodeId);
}

const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_CameraSettings::FindChromakeyOCIOConfiguration(const FString& InClusterNodeId) const
{
	// Always use incamera OCIO
	return CameraOCIO.FindOCIOConfiguration(InClusterNodeId);
}

#if WITH_EDITOR
bool FDisplayClusterConfigurationICVFX_CameraSettings::IsInnerFrustumViewportSettingsEqual_Editor(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return CameraOCIO.IsInnerFrustumViewportSettingsEqual_Editor(InClusterNodeId1, InClusterNodeId2);
}

bool FDisplayClusterConfigurationICVFX_CameraSettings::IsChromakeyViewportSettingsEqual_Editor(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return CameraOCIO.IsChromakeyViewportSettingsEqual_Editor(InClusterNodeId1, InClusterNodeId2);
}

#endif

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_StageSettings
///////////////////////////////////////////////////////////////////////////////////////
const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_StageSettings::FindViewportOCIOConfiguration(const FString& InViewportId) const
{
	return ViewportOCIO.FindOCIOConfiguration(InViewportId);
}

const FOpenColorIOColorConversionSettings* FDisplayClusterConfigurationICVFX_StageSettings::FindLightcardOCIOConfiguration(const FString& InViewportId) const
{
	return Lightcard.LightcardOCIO.FindOCIOConfiguration(InViewportId, ViewportOCIO);
}
