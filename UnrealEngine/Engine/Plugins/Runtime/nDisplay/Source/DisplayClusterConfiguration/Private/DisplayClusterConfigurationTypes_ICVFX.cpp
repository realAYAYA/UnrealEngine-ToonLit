// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_ICVFX.h"

namespace UE::DisplayClusterConfiguration::ICVFX
{
	static float ClampPercent(float InValue)
	{
		static const float MaxCustomFrustumValue = 5.f;

		return FMath::Clamp(InValue, -MaxCustomFrustumValue, MaxCustomFrustumValue);
	}
};
using namespace UE::DisplayClusterConfiguration::ICVFX;

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
	// Note: Lightcard OCIO is enabled from the drop-down menu, so we ignore AllViewportsOCIOConfiguration.bIsEnabled (the property isn't exposed)
	
	// Per viewport OCIO:
	for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : PerViewportOCIOProfiles)
	{
		if (OCIOProfileIt.IsEnabledForObject(InViewportId))
		{
			return &OCIOProfileIt.ColorConfiguration;
		}
	}

	return &AllViewportsOCIOConfiguration.ColorConfiguration;
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

		return &AllViewportsOCIOConfiguration.ColorConfiguration;
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

		return &AllNodesOCIOConfiguration.ColorConfiguration;
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
			if (OCIOProfileIt.bIsEnabled)
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

float FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraFieldOfViewMultiplier(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (CustomFrustum.bEnable)
	{
		return CustomFrustum.FieldOfViewMultiplier;
	}

	return  1.f;
}

float FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraAdaptResolutionRatio(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (CustomFrustum.bAdaptResolution)
	{
		return GetCameraFieldOfViewMultiplier(InStageSettings);
	}

	// Don't use an adaptive resolution multiplier
	return 1.f;
}

float FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraBufferRatio(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	return BufferRatio;
}

bool FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraBorder(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, FLinearColor& OutBorderColor, float& OutBorderThickness) const
{
	if (!Border.Enable)
	{
		OutBorderColor = FLinearColor::Black;
		OutBorderThickness = 0.0f;

		return false;
	}

	const float RealThicknessScaleValue = 0.1f;

	OutBorderColor = Border.Color;
	OutBorderThickness = Border.Thickness * RealThicknessScaleValue;

	return true;
}

FIntPoint FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraFrameSize(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (RenderSettings.CustomFrameSize.bUseCustomSize)
	{
		// Custom camera size
		return FIntPoint(RenderSettings.CustomFrameSize.CustomWidth, RenderSettings.CustomFrameSize.CustomHeight);
	}

	// global camera size
	return FIntPoint(InStageSettings.DefaultFrameSize.Width, InStageSettings.DefaultFrameSize.Height);
}

float FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraFrameAspectRatio(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	FIntPoint FrameSize = GetCameraFrameSize(InStageSettings);

	return (FrameSize.Y > 0 && FrameSize.X > 0) ? (float)FrameSize.X / float(FrameSize.Y) : 0;
}

FVector4 FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraSoftEdge(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	FVector4 ResultSoftEdge(ForceInitToZero);

	const float FieldOfViewMultiplier = GetCameraFieldOfViewMultiplier(InStageSettings);

	// softedge adjustments	
	const float Overscan = (FieldOfViewMultiplier > 0) ? FieldOfViewMultiplier : 1;

	// remap values from 0-1 GUI range into acceptable 0.0 - 0.25 shader range
	ResultSoftEdge.X = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), SoftEdge.Horizontal) / Overscan; // Left
	ResultSoftEdge.Y = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), SoftEdge.Vertical) / Overscan; // Top

	// ZW now used in other way
	// Z for new parameter Feather
	ResultSoftEdge.Z = SoftEdge.Feather;

	// Custom frustum made changes to soft edges
	if (CustomFrustum.bEnable)
	{
		// default - percents
		const float ConvertToPercent = 0.01f;

		float Left = ClampPercent(CustomFrustum.Left * ConvertToPercent);
		float Right = ClampPercent(CustomFrustum.Right * ConvertToPercent);
		float Top = ClampPercent(CustomFrustum.Top * ConvertToPercent);
		float Bottom = ClampPercent(CustomFrustum.Bottom * ConvertToPercent);

		if (CustomFrustum.Mode == EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels)
		{
			const float CameraBufferRatio = GetCameraBufferRatio(InStageSettings);
			const FIntPoint FrameSize = GetCameraFrameSize(InStageSettings);

			const float  FrameWidth = FrameSize.X * CameraBufferRatio;
			const float FrameHeight = FrameSize.Y * CameraBufferRatio;

			Left = ClampPercent(CustomFrustum.Left / FrameWidth);
			Right = ClampPercent(CustomFrustum.Right / FrameWidth);
			Top = ClampPercent(CustomFrustum.Top / FrameHeight);
			Bottom = ClampPercent(CustomFrustum.Bottom / FrameHeight);
		}

		// recalculate soft edge related offsets based on frustum
		ResultSoftEdge.X /= (1 + Left + Right);
		ResultSoftEdge.Y /= (1 + Top + Bottom);
	}

	return ResultSoftEdge;
}

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

EDisplayClusterShaderParametersICVFX_CameraOverlappingRenderMode FDisplayClusterConfigurationICVFX_StageSettings::GetCameraOverlappingRenderMode() const
{
	if (bEnableInnerFrustumChromakeyOverlap)
	{
		return EDisplayClusterShaderParametersICVFX_CameraOverlappingRenderMode::FinalPass;
	}

	return EDisplayClusterShaderParametersICVFX_CameraOverlappingRenderMode::None;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_ChromakeySettings
///////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterShaderParametersICVFX_ChromakeySource FDisplayClusterConfigurationICVFX_ChromakeySettings::GetChromakeyType(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (!bEnable)
	{
		return EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
	}

	switch (ChromakeyType)
	{
	case EDisplayClusterConfigurationICVFX_ChromakeyType::CustomChromakey:
		return EDisplayClusterShaderParametersICVFX_ChromakeySource::ChromakeyLayers;

	default:
		break;
	}

	return EDisplayClusterShaderParametersICVFX_ChromakeySource::FrameColor;
}

FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* FDisplayClusterConfigurationICVFX_ChromakeySettings::GetWritableChromakeyRenderSettings(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings)
{
	// Note: Here we can add an override of the CK rendering settings from StageSetings
	if (GetChromakeyType(InStageSettings) == EDisplayClusterShaderParametersICVFX_ChromakeySource::ChromakeyLayers)
	{
		return &ChromakeyRenderTexture;
	}

	return nullptr;
}

const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* FDisplayClusterConfigurationICVFX_ChromakeySettings::GetChromakeyRenderSettings(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	// Note: Here we can add an override of the CK rendering settings from StageSetings
	if (GetChromakeyType(InStageSettings) == EDisplayClusterShaderParametersICVFX_ChromakeySource::ChromakeyLayers)
	{
		return &ChromakeyRenderTexture;
	}

	return nullptr;
}

const FLinearColor& FDisplayClusterConfigurationICVFX_ChromakeySettings::GetChromakeyColor(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	switch(ChromakeySettingsSource)
	{
	case EDisplayClusterConfigurationICVFX_ChromakeySettingsSource::Viewport:
		// Override Chromakey color from stage settings
		return InStageSettings.GlobalChromakey.ChromakeyColor;

	default:
		break;
	}

	// Use Chromakey color from camera
	return ChromakeyColor;
}

const FLinearColor& FDisplayClusterConfigurationICVFX_ChromakeySettings::GetOverlapChromakeyColor(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	// Note: Here we can add an override of the CK overlap area color from camera

	// Use overlay color from stage settings
	return InStageSettings.GlobalChromakey.ChromakeyColor;
}

const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* FDisplayClusterConfigurationICVFX_ChromakeySettings::ImplGetChromakeyMarkers(const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* InValue) const
{
	// Chromakey markers require texture
	if (!InValue || !InValue->bEnable || InValue->MarkerTileRGBA == nullptr)
	{
		return nullptr;
	}

	// This CK markers can be used
	return InValue;
}

const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* FDisplayClusterConfigurationICVFX_ChromakeySettings::GetChromakeyMarkers(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	switch (ChromakeySettingsSource)
	{
		case EDisplayClusterConfigurationICVFX_ChromakeySettingsSource::Viewport:
			// Use global CK markers
			return ImplGetChromakeyMarkers(&InStageSettings.GlobalChromakey.ChromakeyMarkers);

		default:
			break;
	}

	// Use CK markers from camera
	return ImplGetChromakeyMarkers(&ChromakeyMarkers);
}

const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* FDisplayClusterConfigurationICVFX_ChromakeySettings::GetOverlapChromakeyMarkers(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	// Note: Here we can add an override of the CK overlap markers from camera
	FDisplayClusterConfigurationICVFX_ChromakeyMarkers const* OutChromakeyMarkers = &InStageSettings.GlobalChromakey.ChromakeyMarkers;

	// Use CK overlap markers from stage settings:
	return ImplGetChromakeyMarkers(OutChromakeyMarkers);
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_VisibilityList
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationICVFX_VisibilityList::IsVisibilityListValid() const
{
	for (const FString& ComponentNameIt : RootActorComponentNames)
	{
		if (!ComponentNameIt.IsEmpty())
		{
			return true;
		}
	}

	for (const TSoftObjectPtr<AActor>& ActorSOPtrIt : Actors)
	{
		if (ActorSOPtrIt.IsValid())
		{
			return true;
		}
	}

	for (const FActorLayer& ActorLayerIt : ActorLayers)
	{
		if (!ActorLayerIt.Name.IsNone())
		{
			return true;
		}
	}

	for (const TSoftObjectPtr<AActor>& AutoAddedActor : AutoAddedActors)
	{
		if (AutoAddedActor.IsValid())
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_LightcardSettings
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationICVFX_LightcardSettings::ShouldUseLightCard(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (!bEnable)
	{
		// dont use lightcard if disabled
		return false;
	}

	if (RenderSettings.Replace.bAllowReplace)
	{
		if (RenderSettings.Replace.SourceTexture == nullptr)
		{
			// LightcardSettings.Override require source texture.
			return false;
		}
	}

	// Lightcard require layers for render
	return ShowOnlyList.IsVisibilityListValid();
}

bool FDisplayClusterConfigurationICVFX_LightcardSettings::ShouldUseUVLightCard(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	//Note: Here we can add custom rules for UV lightcards
	return ShouldUseLightCard(InStageSettings);
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings::ShouldUseChromakeyViewport(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (Replace.bAllowReplace)
	{
		if (Replace.SourceTexture == nullptr)
		{
			// ChromakeyRender Override require source texture.
			return false;
		}
	}

	// ChromakeyRender requires actors for render.
	return ShowOnlyList.IsVisibilityListValid();
}
