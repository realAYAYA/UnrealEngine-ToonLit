// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "Components/ActorComponent.h"
#include "ActorLayerUtilities.h"

#include "CineCameraActor.h"

#include "DisplayClusterConfigurationTypes_Media.h"
#include "DisplayClusterConfigurationTypes_PostRender.h"
#include "DisplayClusterConfigurationTypes_Postprocess.h"
#include "DisplayClusterConfigurationTypes_OCIO.h"

#include "DisplayClusterConfigurationTypes_ICVFX.generated.h"

USTRUCT(Blueprintable, meta = (DefaultSubstitutions = "LayersTooltip = Actor Layers, ActorsTooltip = Actor references"))
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_VisibilityList
{
	GENERATED_BODY()

public:
	/** Actor Layers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Layers", ToolTip = "{LayersTooltip}"))
	TArray<FActorLayer> ActorLayers;

	/** Actor references */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ToolTip = "{ActorsTooltip}"))
	TArray<TSoftObjectPtr<AActor>> Actors;

	/** Reference to RootActor components by names */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = NDisplay)
	TArray<FString> RootActorComponentNames; //@todo change link, now by names
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CustomSize
{
	GENERATED_BODY()

public:
	// Use custom size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	bool bUseCustomSize = false;

	// Used when enabled "bUseCustomSize"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "32", UIMin = "32", EditCondition = "bUseCustomSize"))
	int CustomWidth = 2560;

	// Used when enabled "bUseCustomSize"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "32", UIMin = "32", EditCondition = "bUseCustomSize"))
	int CustomHeight = 1440;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_Size
{
	GENERATED_BODY()

public:
	// Viewport width in pixels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "32", UIMin = "32"))
	int Width = 2560;

	// Viewport height  in pixels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "32", UIMin = "32"))
	int Height = 1440;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_OverlayAdvancedRenderSettings
{
	GENERATED_BODY()

public:
	// Allow ScreenPercentage 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.05", UIMin = "0.05", ClampMax = "10.0", UIMax = "10.0"))
	float BufferRatio = 1;

	// Performance: Render to scale RTT, resolved with shader to viewport (Custom value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float RenderTargetRatio = 1;

	// Performance, Multi-GPU: Asign GPU for viewport rendering. The Value '-1' used to default gpu mapping (EYE_LEFT and EYE_RIGHT GPU)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	int GPUIndex = -1;

	// Performance, Multi-GPU: Customize GPU for stereo mode second view (EYE_RIGHT GPU)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	int StereoGPUIndex = -1;

	// Performance: force monoscopic render, resolved to stereo viewport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	EDisplayClusterConfigurationViewport_StereoMode StereoMode = EDisplayClusterConfigurationViewport_StereoMode::Default;

	// Experimental: Support special frame builder mode - merge viewports to single viewfamily by group num
	// [not implemented yet]
	UPROPERTY()
	int RenderFamilyGroup = -1;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings
{
	GENERATED_BODY()

public:
	/** Set to True to use custom chromakey content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Use Custom Chromakey"))
	bool bEnable = false;

	// Replace the texture of the camera viewport from this chromakey RTT
	UPROPERTY(BlueprintReadWrite,Category = NDisplay, meta = (EditCondition = "bEnable"))
	bool bReplaceCameraViewport = false;

	// Performance: Use custom size (low-res) for chromakey RTT frame. Default size same as camera frame
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CustomSize CustomSize;

	/** Content specified here will be overridden to use the chromakey color specified and include chromakey markers if enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable", DisplayName = "Custom Chromakey Content"))
	FDisplayClusterConfigurationICVFX_VisibilityList ShowOnlyList;

	// Replace viewport render from source texture
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationPostRender_Override Replace;

	/** Apply blur to the Custom Chromakey content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable", DisplayName = "Post Process Blur"))
	FDisplayClusterConfigurationPostRender_BlurPostprocess PostprocessBlur;

	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationPostRender_GenerateMips GenerateMips;

	// Advanced render settings
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_OverlayAdvancedRenderSettings AdvancedRenderSettings;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_ChromakeyMarkers
{
	GENERATED_BODY()

public:
	FDisplayClusterConfigurationICVFX_ChromakeyMarkers();

public:
	/** True to display Chromakey Markers within the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Enable Chromakey Markers"))
	bool bEnable = true;

	/** Marker Color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FLinearColor MarkerColor;

	/** Texture to use as the chromakey marker tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	TObjectPtr<UTexture> MarkerTileRGBA = nullptr;

	/** Scale value for the size of each chromakey marker tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable", ClampMin = "0", UIMin = "0", DisplayName = "Marker Scale"))
	float MarkerSizeScale = 0.5;

	/** Distance value between each chromakey marker tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable", ClampMin = "0", UIMin = "0"))
	float MarkerTileDistance = 1.5;

	/** Offset value for the chromakey marker tiles, normalized to the tile distance.  Adjust placement of the chromakey markers within the composition of the camera framing.  Whole numbers will offset chromakey markers by a cyclical amount and have no visual change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FVector2D MarkerTileOffset;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_ChromakeySettings
{
	GENERATED_BODY()

public:
	FDisplayClusterConfigurationICVFX_ChromakeySettings()
		// Default chromakey color is (0,128,0)
		: ChromakeyColor(0, 0.5f, 0)
	{ }

public:
	/** Set to True to fill the inner frustum with the specified Chromakey Color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Enable Inner Frustum Chromakey"))
	bool bEnable = false;

	/** Chromakey Color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FLinearColor ChromakeyColor;

	/** Configure a custom chromakey based on content that will appear in the inner frustum, rather than the entire inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable", DisplayName = "Custom Chromakey"))
	FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings ChromakeyRenderTexture;

	/** Display Chromakey Markers to facilitate camera tracking in post production. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_ChromakeyMarkers ChromakeyMarkers;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_LightcardRenderSettings
{
	GENERATED_BODY()

public:
	// override the texture of the target viewport from this lightcard RTT
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	bool bReplaceViewport = false;

	// Override viewport render from source texture
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	FDisplayClusterConfigurationPostRender_Override Replace;

	UPROPERTY()
	FDisplayClusterConfigurationPostRender_BlurPostprocess PostprocessBlur;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	FDisplayClusterConfigurationPostRender_GenerateMips GenerateMips;

	// Advanced render settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	FDisplayClusterConfigurationICVFX_OverlayAdvancedRenderSettings AdvancedRenderSettings;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_LightcardSettings
{
	GENERATED_BODY()

public:
	/** Enable Light Cards */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Cards", meta = (DisplayName = "Enable Light Cards"))
	bool bEnable = true;

	/** Enable\Disable freeze rendering for lightcards when outer viewports rendering also freezed. This will impact performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Cards", meta = (DisplayName = "Ignore Freezing of Outer Viewports for Lightcards"))
	bool bIgnoreOuterViewportsFreezingForLightcards = true;

	/** Specify how to render Light Cards in relation to the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Cards", meta = (DisplayName = "Blending Mode", EditCondition = "bEnable"))
	EDisplayClusterConfigurationICVFX_LightcardRenderMode Blendingmode = EDisplayClusterConfigurationICVFX_LightcardRenderMode::Under;

	// Render actors from this layers to lightcard textures
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Cards", meta = (EditCondition = "bEnable", DisplayName = "Light Cards Content", ToolTip = "Content specified here will be treated as a Light Card and adhere to the Blending Mode setting", Substitutions = "LayersTooltip = Layers containing Light Cards, ActorsTooltip = Light Card Actors"))
	FDisplayClusterConfigurationICVFX_VisibilityList ShowOnlyList;

	// Configure global render settings for this viewport
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_LightcardRenderSettings RenderSettings;

	// Enable using outer viewport OCIO from DCRA for lightcard rendering
	UPROPERTY()
	bool bEnableOuterViewportOCIO = false;

	// Enable using outer viewport Color Grading from DCRA for lightcard rendering
	UPROPERTY()
	bool bEnableOuterViewportColorGrading = false;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings
{
	GENERATED_BODY()

public:
	// Performance: Render to scale RTT, resolved with shader to viewport (Custom value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float RenderTargetRatio = 1;

	// Performance, Multi-GPU: Asign GPU for viewport rendering. The Value '-1' used to default gpu mapping (EYE_LEFT and EYE_RIGHT GPU)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	int GPUIndex = -1;

	// Performance, Multi-GPU: Customize GPU for stereo mode second view (EYE_RIGHT GPU)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	int StereoGPUIndex = -1;

	// Performance: force monoscopic render, resolved to stereo viewport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	EDisplayClusterConfigurationViewport_StereoMode StereoMode = EDisplayClusterConfigurationViewport_StereoMode::Default;

	// Experimental: Support special frame builder mode - merge viewports to single viewfamily by group num
	// [not implemented yet]
	UPROPERTY()
	int RenderFamilyGroup = -1;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraRenderSettings
{
	GENERATED_BODY()

public:
	FDisplayClusterConfigurationICVFX_CameraRenderSettings();

public:
	// Define custom inner camera viewport size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	FDisplayClusterConfigurationICVFX_CustomSize CustomFrameSize;

	// Camera render order, bigger value is over
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	int RenderOrder = -1;

	UPROPERTY()
	FDisplayClusterConfigurationViewport_CustomPostprocess CustomPostprocess;

	// Use postprocess settings from camera component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	bool bUseCameraComponentPostprocess = true;

	// Replace viewport render from source texture
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Replace Output With a Texture"))
	FDisplayClusterConfigurationPostRender_Override Replace;

	// Media settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Media"))
	FDisplayClusterConfigurationMedia Media;

	UPROPERTY()
	FDisplayClusterConfigurationPostRender_BlurPostprocess PostprocessBlur;

	/** Mipmapping can help avoid visual artifacts when the inner frustum is rendered at a lower resolution than specified in the configuration and is smaller on screen than the available pixels on the display device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Mipmapping", Simplified))
	FDisplayClusterConfigurationPostRender_GenerateMips GenerateMips;

	// Advanced render settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	FDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings AdvancedRenderSettings;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraMotionBlurOverridePPS
{
	GENERATED_BODY()

public:
	/** If enabled, override the overall motion blur settings that would otherwise come from the current post-process volume or Cine Camera. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = NDisplay, meta = (DisplayName = "Enable Settings Override"))
	bool bReplaceEnable = false;

	/** Strength of motion blur, 0:off. */
	UPROPERTY(interp, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bReplaceEnable", DisplayName = "Intensity"))
	float MotionBlurAmount = 1;

	/** Max distortion caused by motion blur in percent of the screen width, 0:off */
	UPROPERTY(interp, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.0", ClampMax = "100.0", editcondition = "bReplaceEnable", DisplayName = "Max"))
	float MotionBlurMax = 50;

	/** The minimum projected screen radius for a primitive to be drawn in the velocity pass.Percentage of screen width, smaller numbers cause more draw calls, default: 4 % */
	UPROPERTY(interp, BlueprintReadWrite, Category = NDisplay, meta = (ClampMin = "0.0", UIMax = "100.0", editcondition = "bReplaceEnable", DisplayName = "Per Object Size"))
	float MotionBlurPerObjectSize = 4;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraMotionBlur
{
	GENERATED_BODY()

public:
	/** Specify the motion blur mode for the inner frustum, correcting for the motion of the camera. Blur due to camera motion will be incorrectly doubled in the physically exposed image if there is already camera blur applied to the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	EDisplayClusterConfigurationCameraMotionBlurMode MotionBlurMode = EDisplayClusterConfigurationCameraMotionBlurMode::Override;

	/** Translation Scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay)
	float TranslationScale = 1.f;

	/** Motion Blur Settings Override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Motion Blur Settings Override"))
	FDisplayClusterConfigurationICVFX_CameraMotionBlurOverridePPS MotionBlurPPS;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraSoftEdge
{
	GENERATED_BODY()

	/** Adjust blur amount to the top and bottom edges of the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Top and Bottom", ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float Vertical = 0.f;

	/** Adjust blur amount to the left and right side edges of the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Sides", ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float Horizontal = 0.f;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraBorder
{
	GENERATED_BODY()

	/** Enable Innder Frustum Border. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Enable Inner Frustum Border"))
	bool Enable = false;

	/** Adjust border width to the top and bottom edges of the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Border Width", ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float Thickness = 0.05f;

	/** Adjust color of the border edges of the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Border Color"))
	FLinearColor Color = FLinearColor::Blue;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraCustomFrustum
{
	GENERATED_BODY()

	FDisplayClusterConfigurationICVFX_CameraCustomFrustum();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = NDisplay)
	FIntPoint EstimatedOverscanResolution;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = NDisplay)
	FIntPoint InnerFrustumResolution;

	UPROPERTY(BlueprintReadOnly, Category = NDisplay, meta = (HideInDetailPanel))
	float OverscanPixelsIncrease;

	/** Enable Custom Frustum Frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Enable Inner Frustum Overscan"))
	bool bEnable = false;

	/** Enable Custom Frustum Frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Adapt Resolution", EditCondition = "bEnable"))
	bool bAdaptResolution = false;

	/** Multiply the field of view for the ICVFX camera by this value.  This can increase the overall size of the inner frustum to help provide a buffer against latency when moving the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Overscan Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "5.0", UIMax = "5.0", EditCondition = "bEnable"))
	float FieldOfViewMultiplier = 1.0f;

	/** Enable/disable inner camera custom frustum and specify units as percent or pixel values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Overscan Units", EditCondition = "bEnable"))
	EDisplayClusterConfigurationViewportCustomFrustumMode Mode = EDisplayClusterConfigurationViewportCustomFrustumMode::Percent;

	/** Pixel/Percent value to alter the frustum to the left side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Left", ClampMin = "-500.0", UIMin = "-500.0", ClampMax = "500.0", UIMax = "500.0", EditCondition = "bEnable"))
	float Left = 0;

	/** Pixel/Percent value to alter the frustum to the right side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Right", ClampMin = "-500.0", UIMin = "-500.0", ClampMax = "500.0", UIMax = "500.0", EditCondition = "bEnable"))
	float Right = 0;

	/** Pixel/Percent value to alter the frustum to the top*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Top", ClampMin = "-500.0", UIMin = "-500.0", ClampMax = "500.0", UIMax = "500.0", EditCondition = "bEnable"))
	float Top = 0;

	/** Pixel/Percent value to alter the frustum to the bottom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Bottom", ClampMin = "-500.0", UIMin = "-500.0", ClampMax = "500.0", UIMax = "500.0", EditCondition = "bEnable"))
	float Bottom = 0;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_CameraSettings
{
	GENERATED_BODY()

	FDisplayClusterConfigurationICVFX_CameraSettings();

public:
	/** Render the inner frustum for this ICVFX camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Enable Inner Frustum"))
	bool bEnable = true;

	/** Specify a Cine Camera Actor for this ICVFX camera to use instead of the default nDisplay camera. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "In Camera VFX", meta = (DisplayName = "Cine Camera Actor", EditCondition = "bEnable"))
	TSoftObjectPtr<ACineCameraActor> ExternalCameraActor;

	/** Adjust resolution scaling for the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Inner Frustum Screen Percentage", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10.0", UIMax = "1.0", EditCondition = "bEnable"))
	float BufferRatio = 1;

	/** Render a larger or smaller inner frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Inner Frustum Overscan", EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CameraCustomFrustum CustomFrustum;

	/** Soften the edges of the inner frustum to help avoid hard lines in reflections seen by the live-action camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CameraSoftEdge SoftEdge;

	/** Rotate the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Inner Frustum Rotation", EditCondition = "bEnable"))
	FRotator  FrustumRotation = FRotator::ZeroRotator;

	/** Specify an offset on the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Inner Frustum Offset", EditCondition = "bEnable"))
	FVector FrustumOffset = FVector::ZeroVector;

	/**Border for the inner frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Inner Frustum Border", EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CameraBorder Border;

	/** Render motion blur more accurately by subtracting blur from camera motion and avoiding amplification of blur by the physical camera. */
	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, EditAnywhere, Category = "In Camera VFX", meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CameraMotionBlur CameraMotionBlur;

	/** Configure global render settings for this viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_CameraRenderSettings RenderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Chromakey, meta = (EditCondition = "bEnable"))
	FDisplayClusterConfigurationICVFX_ChromakeySettings Chromakey;

	/** OCIO Display look configuration for this camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", meta = (DisplayName = "All Nodes OCIO Configuration", EditCondition = "bEnable"))
	FDisplayClusterConfigurationOCIOConfiguration AllNodesOCIOConfiguration;

	/** Apply an OpenColorIO configuration on a per-node or group-of-nodes basis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", meta = (DisplayName = "Per-Node OCIO Overrides", ConfigurationMode = "ClusterNodes", EditCondition = "bEnable"))
	TArray<FDisplayClusterConfigurationOCIOProfile> PerNodeOCIOProfiles;

	/** Entire Cluster Color Grading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inner Frustum Color Grading", meta = (DisplayName = "Enable Inner Frustum Color Grading"))
	bool EnableInnerFrustumColorGrading = true;

	/** All Nodes Color Grading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inner Frustum Color Grading", meta = (DisplayName = "All Nodes Color Grading", EditCondition = "bEnable"))
	FDisplayClusterConfigurationViewport_AllNodesColorGrading AllNodesColorGrading;

	/** Perform advanced color grading operations for the inner frustum on a per-node or group-of-nodes basis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inner Frustum Color Grading", meta = (DisplayName = "Per-Node Color Grading", ConfigurationMode = "ClusterNodes", EditCondition = "bEnable"))
	TArray<FDisplayClusterConfigurationViewport_PerNodeColorGrading> PerNodeColorGrading;

	/** Content specified here will not appear in the inner frustum, but can appear in the nDisplay viewports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Content Hidden from Inner Frustum", EditCondition = "bEnable", Substitutions = "LayersTooltip = Layers hidden from the inner frustum, ActorsTooltip = Actors hidden from the inner frustum"))
	FDisplayClusterConfigurationICVFX_VisibilityList CameraHideList;

	/** A list of viewports that the inner frustum is not rendered to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (ClusterItemType = Viewports, DisplayName = "Inner Frustum Hidden in Viewports", EditCondition = "bEnable"))
	FDisplayClusterConfigurationClusterItemReferenceList HiddenICVFXViewports;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationICVFX_StageSettings
{
	GENERATED_BODY()

public:
	/** Enable/disable the inner frustum on all ICVFX cameras. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (DisplayName = "Enable Inner Frustum"))
	bool bEnableInnerFrustums = true;

	/** Default incameras RTT texture size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Configuration)
	FDisplayClusterConfigurationICVFX_Size DefaultFrameSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Cards", meta = (ExpandProperties))
	FDisplayClusterConfigurationICVFX_LightcardSettings Lightcard;

	/** Freeze rendering for viewports. This improves performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Viewports, meta = (DisplayName = "Freeze Viewports"))
	bool bFreezeRenderOuterViewports = false;

	// Hide list for all icvfx viewports (outer, inner, cameras, etc)
	// (This allow to hide all actors from layers for icvfx render logic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Viewports, meta = (DisplayName = "Content Hidden from Entire Cluster", ToolTip = "Content specified here will not appear anywhere in the nDisplay cluster", Substitutions = "LayersTooltip = Layers hidden from the entire nDisplay cluster, ActorsTooltip = Actors hidden from the entire nDisplay cluster"))
	FDisplayClusterConfigurationICVFX_VisibilityList HideList;

	/** Special hide list for Outer viewports */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Viewports, meta = (DisplayName = "Content Hidden from Viewports", ToolTip = "Content specified here will not appear in the nDisplay viewports, but can appear in the inner frustum.", Substitutions = "LayersTooltip = Layers hidden from the nDisplay viewports, ActorsTooltip = Actors hidden from the nDisplay viewports"))
	FDisplayClusterConfigurationICVFX_VisibilityList OuterViewportHideList;

	/** Viewport Color Grading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewport Color Grading", meta = (DisplayName = "Enable Color Grading"))
	bool EnableColorGrading = true;

	/** Entire Cluster Color Grading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewport Color Grading", meta = (DisplayName = "Entire Cluster"))
	FDisplayClusterConfigurationViewport_EntireClusterColorGrading EntireClusterColorGrading;

	/** Perform advanced color grading operations on a per-viewport or group-of-viewports basis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewport Color Grading", meta = (DisplayName = "Per-Viewport Color Grading", ConfigurationMode = "Viewports"))
	TArray<FDisplayClusterConfigurationViewport_PerViewportColorGrading> PerViewportColorGrading;

	/** Enable the application of an OpenColorIO configuration to all viewports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", meta = (DisplayName = "Enable Viewport OCIO"))
	bool bUseOverallClusterOCIOConfiguration = true;

	/** Apply this OpenColorIO configuration to all viewports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", meta = (DisplayName = "All Viewports Color Configuration", EditCondition = "bUseOverallClusterOCIOConfiguration"))
	FDisplayClusterConfigurationOCIOConfiguration AllViewportsOCIOConfiguration;

	/** Apply an OpenColorIO configuration on a per-viewport or group-of-viewports basis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", meta = (DisplayName = "Per-Viewport OCIO Overrides", ConfigurationMode = "Viewports", EditCondition = "bUseOverallClusterOCIOConfiguration"))
	TArray<FDisplayClusterConfigurationOCIOProfile> PerViewportOCIOProfiles;
};
