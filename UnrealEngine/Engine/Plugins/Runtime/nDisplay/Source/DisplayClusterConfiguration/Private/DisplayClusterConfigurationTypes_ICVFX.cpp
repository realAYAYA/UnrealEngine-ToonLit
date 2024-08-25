// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes.h"
#include "IDisplayCluster.h"
#include "Camera/CameraTypes.h"
#include "CineCameraComponent.h"

namespace UE::DisplayClusterConfiguration::ICVFX
{
	static float ClampPercent(float InValue)
	{
		static const float MaxCustomFrustumValue = 5.f;

		return FMath::Clamp(InValue, -MaxCustomFrustumValue, MaxCustomFrustumValue);
	}

	static float ClampCustomFrustum(float InValue, float InMax)
	{
		return FMath::Clamp(InValue, -InMax, InMax);
	}
};
using namespace UE::DisplayClusterConfiguration::ICVFX;

int32 GDisplayClusterICVFXCameraAdoptResolution = 1;
static FAutoConsoleVariableRef CVarGDisplayClusterICVFXCameraAdoptResolution(
	TEXT("nDisplay.icvfx.camera.AdoptResolution"),
	GDisplayClusterICVFXCameraAdoptResolution,
	TEXT("Adopt camera viewport resolution with 'Filmback + CropSettings + SqueezeFactor' CineCamera settings.  (Default = 1)"),
	ECVF_Default
);

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

void FDisplayClusterConfigurationICVFX_CameraRenderSettings::SetupViewInfo(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, FMinimalViewInfo& InOutViewInfo) const
{
	// CameraSettings can disable posprocess from this camera
	if (!bUseCameraComponentPostprocess)
	{
		InOutViewInfo.PostProcessSettings = FPostProcessSettings();
		InOutViewInfo.PostProcessBlendWeight = 0.0f;
	}
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

bool FDisplayClusterConfigurationICVFX_CameraOCIO::IsChromakeyViewportSettingsEqual(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return IsInnerFrustumViewportSettingsEqual(InClusterNodeId1, InClusterNodeId2);
}

bool FDisplayClusterConfigurationICVFX_CameraOCIO::IsInnerFrustumViewportSettingsEqual(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
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

bool FDisplayClusterConfigurationICVFX_CameraSettings::IsICVFXEnabled(const UDisplayClusterConfigurationData& InConfigurationData, const FString& InClusterNodeId) const
{
	// When rendering offscreen, we have an extended logic for camera rendering activation
	static const bool bIsRunningClusterModeOffscreen =
		(IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster) &&
		FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));

	if (!bIsRunningClusterModeOffscreen)
	{
		return bEnable;
	}

	if (!bEnable)
	{
		return false;
	}

	// If cluster mode + rendering offscreen, discover media output settings

	// First condition to render offscreen: it has media output assigned
	const bool bUsesMediaOutput = RenderSettings.Media.bEnable && (
			RenderSettings.Media.HasAnyMediaOutputAssigned(InClusterNodeId, EDisplayClusterConfigurationMediaSplitType::FullFrame) ||
			RenderSettings.Media.HasAnyMediaOutputAssigned(InClusterNodeId, EDisplayClusterConfigurationMediaSplitType::UniformTiles));

	// Get backbuffer media settings
	const UDisplayClusterConfigurationClusterNode* const NodeCfg = InConfigurationData.Cluster->GetNode(InClusterNodeId);
	const FDisplayClusterConfigurationMediaNodeBackbuffer* BackbufferMediaSettings = NodeCfg ? &NodeCfg->MediaSettings : nullptr;

	// Second condition to render offscreen: the backbuffer has media output assigned.
	// This means the whole frame including ICVFX cameras need to be rendered.
	const bool bIsBackbufferBeingCaptured = BackbufferMediaSettings ? BackbufferMediaSettings->bEnable && BackbufferMediaSettings->IsMediaOutputAssigned() : false;

	// Finally make a decision if the camera should be rendered
	return bUsesMediaOutput || bIsBackbufferBeingCaptured;
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

bool FDisplayClusterConfigurationICVFX_CameraSettings::IsInnerFrustumViewportSettingsEqual(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return CameraOCIO.IsInnerFrustumViewportSettingsEqual(InClusterNodeId1, InClusterNodeId2);
}

bool FDisplayClusterConfigurationICVFX_CameraSettings::IsChromakeyViewportSettingsEqual(const FString& InClusterNodeId1, const FString& InClusterNodeId2) const
{
	return CameraOCIO.IsChromakeyViewportSettingsEqual(InClusterNodeId1, InClusterNodeId2);
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

void FDisplayClusterConfigurationICVFX_CameraSettings::SetupViewInfo(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, FMinimalViewInfo& InOutViewInfo)
{
	RenderSettings.SetupViewInfo(InStageSettings, InOutViewInfo);
	CustomFrustum.SetupViewInfo(InStageSettings, *this, InOutViewInfo);
	CameraMotionBlur.SetupViewInfo(InStageSettings, InOutViewInfo);
}

FIntPoint FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraFrameSize(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const UCineCameraComponent& InCineCameraComponent) const
{
	const FIntPoint CameraFrameSize = RenderSettings.CustomFrameSize.bUseCustomSize
		? FIntPoint(RenderSettings.CustomFrameSize.CustomWidth, RenderSettings.CustomFrameSize.CustomHeight)
		: FIntPoint(InStageSettings.DefaultFrameSize.Width, InStageSettings.DefaultFrameSize.Height);

	if (GDisplayClusterICVFXCameraAdoptResolution)
	{
		// Get the size of the cinematic camera's cropped sensor:
		const double CropedSensorWidth  = FMath::Tan(FMath::DegreesToRadians(InCineCameraComponent.GetHorizontalFieldOfView()) / 2.f) * 2.f * InCineCameraComponent.CurrentFocalLength;
		const double CropedSensorHeight = FMath::Tan(FMath::DegreesToRadians(InCineCameraComponent.GetVerticalFieldOfView()) / 2.f) * 2.f * InCineCameraComponent.CurrentFocalLength;

		// Get the ratio of the cinematic camera's cropped sensor size to the base sensor size.
		const double CroppedSensorWidthRatio  = CropedSensorWidth / InCineCameraComponent.Filmback.SensorWidth;
		const double CroppedSensorHeightRatio = CropedSensorHeight / InCineCameraComponent.Filmback.SensorHeight;

		// Adapt camera resolution to the filmback sensor aspect ratio
		// We keep the width, but adjust the height to match the aspect ratio of the Fimlmback sensor.
		const double CameraFrameHeight = (InCineCameraComponent.Filmback.SensorHeight > 0.f && InCineCameraComponent.Filmback.SensorWidth > 0.f)
			? CameraFrameSize.X / (InCineCameraComponent.Filmback.SensorWidth / InCineCameraComponent.Filmback.SensorHeight)
			: CameraFrameSize.Y;

		// Get cropped camera size
		const FIntPoint CroppedCameraFrameSize(
			FMath::RoundToInt(CameraFrameSize.X * CroppedSensorWidthRatio),
			FMath::RoundToInt(CameraFrameHeight * CroppedSensorHeightRatio)
		);

		return CroppedCameraFrameSize;
	}

	return CameraFrameSize;
}

float FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraFrameAspectRatio(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const UCineCameraComponent& InCineCameraComponent) const
{
	FIntPoint FrameSize = GetCameraFrameSize(InStageSettings, InCineCameraComponent);

	return (FrameSize.Y > 0 && FrameSize.X > 0) ? (float)FrameSize.X / float(FrameSize.Y) : 0;
}

FVector4 FDisplayClusterConfigurationICVFX_CameraSettings::GetCameraSoftEdge(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const UCineCameraComponent& InCineCameraComponent) const
{
	FVector4 ResultSoftEdge(ForceInitToZero);

	const float FieldOfViewMultiplier = CustomFrustum.GetCameraFieldOfViewMultiplier(InStageSettings);

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
			const FIntPoint FrameSize = GetCameraFrameSize(InStageSettings, InCineCameraComponent);

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

void FDisplayClusterConfigurationICVFX_CameraDepthOfField::UpdateDynamicCompensationLUT()
{
	CompensationLUT.LoadSynchronous();
	if (CompensationLUT)
	{
		FSharedImageConstRef CPUTextureRef = CompensationLUT->GetCPUCopy();
		if (CPUTextureRef.IsValid() && CPUTextureRef->Format == ERawImageFormat::R32F)
		{
			TArrayView64<const float> SrcPixels = CPUTextureRef->AsR32F();
			TArray64<FFloat16> DestPixels;
			DestPixels.AddZeroed(SrcPixels.Num());

			for (int32 Index = 0; Index < SrcPixels.Num(); ++Index)
			{
				// Scale the offset encoded in the LUT so that the final CoC when computed in the DoF pipeline is scaled by the gain.
				// The actual new offset needed to accomplish this comes from the following equation:
				// c * (CoC_obj + CoC_off) = CoC_obj + newOffset =>
				// newOffset = (1 - c) * CoC_obj + c * CoC_off
				
				const float ObjectCoC = Index / 32.0f + 1;
				const float Offset = SrcPixels[Index];
				const float ScaledOffset = (1 - DepthOfFieldGain) * ObjectCoC + DepthOfFieldGain * Offset;
				DestPixels[Index] = ScaledOffset;
			}

			TArrayView<uint8> PixelsView((uint8*)DestPixels.GetData(), int64(DestPixels.Num() * sizeof(FFloat16)));

			// Texture format is assumed to be greyscale (PF_G8), and we must disable sRGB on the texture to ensure the raw byte value, which encodes
			// the offset in pixels, is passed unmodified to the depth of field shader
			if (UTexture2D* NewTexture = UTexture2D::CreateTransient(CPUTextureRef->GetWidth(), CPUTextureRef->GetHeight(), PF_R16F, NAME_None, PixelsView))
			{
				DynamicCompensationLUT = NewTexture;
				DynamicCompensationLUT->SRGB = 0;
				return;
			}
		}
	}

	DynamicCompensationLUT = nullptr;
}

void FDisplayClusterConfigurationICVFX_CameraCustomFrustum::SetupViewInfo(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings, FMinimalViewInfo& InOutViewInfo) const
{
	// Since Circle of confusion is directly proportional to aperature, with wider FOV focal length needs to be shortened by the same amount as FOV.
	if (bEnable && InCameraSettings.ExternalCameraActor.IsValid())
	{
		// default - percents
		const float ConvertToPercent = 0.01f;
		const float MaxPercentOverscan = .5f;
		const float MaxPixelOverscan = 5.f;

		float LocLeft = Left;
		float LocRight = Right;
		float LocTop = Top;
		float LocBottom = Bottom;

		if (Mode == EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels)
		{
			LocLeft = ClampCustomFrustum(Left * ConvertToPercent, MaxPixelOverscan);
			LocRight = ClampCustomFrustum(Right * ConvertToPercent, MaxPixelOverscan);
			LocTop = ClampCustomFrustum(Top * ConvertToPercent, MaxPixelOverscan);
			LocBottom = ClampCustomFrustum(Bottom * ConvertToPercent, MaxPixelOverscan);

			const float CameraBufferRatio = InCameraSettings.GetCameraBufferRatio(InStageSettings);
			const FIntPoint FrameSize = InCameraSettings.GetCameraFrameSize(InStageSettings, *InCameraSettings.ExternalCameraActor->GetCineCameraComponent());
			const float  FrameWidth = FrameSize.X * CameraBufferRatio;
			const float FrameHeight = FrameSize.Y * CameraBufferRatio;

			LocLeft = LocLeft / FrameWidth;
			LocRight = LocRight / FrameWidth;
			LocTop = LocTop / FrameHeight;
			LocBottom = LocBottom / FrameHeight;
		}

		LocLeft = ClampCustomFrustum(LocLeft * ConvertToPercent, MaxPercentOverscan);
		LocRight = ClampCustomFrustum(LocRight * ConvertToPercent, MaxPercentOverscan);
		LocTop = ClampCustomFrustum(LocTop * ConvertToPercent, MaxPercentOverscan);
		LocBottom = ClampCustomFrustum(LocBottom * ConvertToPercent, MaxPercentOverscan);

		const float FOVMultiplier = GetCameraFieldOfViewMultiplier(InStageSettings);
		const float ClampedFieldOfViewMultiplier = (FOVMultiplier > 0.f) ? FOVMultiplier : 1.f;
		InOutViewInfo.PostProcessSettings.DepthOfFieldMinFstop /= ClampedFieldOfViewMultiplier;
		InOutViewInfo.PostProcessSettings.DepthOfFieldFstop /= ClampedFieldOfViewMultiplier;

		float Multiplier = (1. + (LocLeft + LocRight) / 2.) * (1. + (LocTop + LocBottom) / 2.);
		InOutViewInfo.PostProcessSettings.DepthOfFieldMinFstop /= Multiplier;
		InOutViewInfo.PostProcessSettings.DepthOfFieldFstop /= Multiplier;
	}
}

float FDisplayClusterConfigurationICVFX_CameraCustomFrustum::GetCameraFieldOfViewMultiplier(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (bEnable)
	{
		return FieldOfViewMultiplier;
	}

	return  1.f;
}

float FDisplayClusterConfigurationICVFX_CameraCustomFrustum::GetCameraAdaptResolutionRatio(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	if (bAdaptResolution)
	{
		return GetCameraFieldOfViewMultiplier(InStageSettings);
	}

	// Don't use an adaptive resolution multiplier
	return 1.f;
}

void FDisplayClusterConfigurationICVFX_CameraMotionBlur::SetupViewInfo(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, FMinimalViewInfo& InOutViewInfo) const
{
	// Add postprocess blur settings to viewinfo PP
	if (MotionBlurPPS.bReplaceEnable)
	{
		// Send camera postprocess to override
		InOutViewInfo.PostProcessBlendWeight = 1.0f;

		InOutViewInfo.PostProcessSettings.MotionBlurAmount = MotionBlurPPS.MotionBlurAmount;
		InOutViewInfo.PostProcessSettings.bOverride_MotionBlurAmount = true;

		InOutViewInfo.PostProcessSettings.MotionBlurMax = MotionBlurPPS.MotionBlurMax;
		InOutViewInfo.PostProcessSettings.bOverride_MotionBlurMax = true;

		InOutViewInfo.PostProcessSettings.MotionBlurPerObjectSize = MotionBlurPPS.MotionBlurPerObjectSize;
		InOutViewInfo.PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
	}
}
