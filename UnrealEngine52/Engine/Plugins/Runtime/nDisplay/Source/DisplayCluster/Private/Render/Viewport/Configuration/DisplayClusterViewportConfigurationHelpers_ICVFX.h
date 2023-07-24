// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ShaderParameters/DisplayClusterShaderParameters_ICVFX.h"
#include "IDisplayClusterProjection.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

class FDisplayClusterViewport;
class FDisplayClusterViewportManager;
class ADisplayClusterRootActor;
class UDisplayClusterICVFXCameraComponent;

struct FDisplayClusterConfigurationICVFX_ChromakeySettings;
struct FDisplayClusterConfigurationICVFX_ChromakeyMarkers;
struct FDisplayClusterConfigurationICVFX_LightcardSettings;
struct FDisplayClusterConfigurationICVFX_CameraCustomFrustum;
struct FDisplayClusterConfigurationICVFX_CameraSettings;

struct FCameraContext_ICVFX
{
	//@todo: add stereo context support
	FRotator ViewRotation;
	FVector  ViewLocation;
	FMatrix  PrjMatrix;
};

class FDisplayClusterViewportConfigurationHelpers_ICVFX
{
public:
	/* Get the ViewportManager object from DCRA. */
	static FDisplayClusterViewportManager* GetViewportManager(ADisplayClusterRootActor& RootActor);

	/* Find existing InnerFrustum viewport. */
	static FDisplayClusterViewport* FindCameraViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);

	/* Return the new or existing InnerFrustum viewport. */
	static FDisplayClusterViewport* GetOrCreateCameraViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);

	/* Return the new or existing Chromakey viewport. */
	static FDisplayClusterViewport* GetOrCreateChromakeyViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
	
	/* Return the new or existing LightCard viewport. */
	static FDisplayClusterViewport* GetOrCreateLightcardViewport(FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor);

	/* Return the new or existing UVLightCard viewport. */
	static FDisplayClusterViewport* GetOrCreateUVLightcardViewport(FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor);

	/* Because UVLightCard viewports share the same outer texture (from LightCardManager), they can clone each other.
	 * Except when the OCIO settings are not equal.
	 * This function sets the InUVLightCardViewport rendering settings to minimize rendering costs.
	 */
	static void ReuseUVLightCardViewportWithinClusterNode(FDisplayClusterViewport& InUVLightCardViewport);

	/* Returns true if the use of the LightCard viewport is allowed in InLightcardSettings.
	 * These viewports also require visible LightCard actors to render.
	 */
	static bool IsShouldUseLightcard(const FDisplayClusterConfigurationICVFX_LightcardSettings& InLightcardSettings);

	/* Returns true if the use of the UVLightCard viewport is allowed in InLightcardSettings.
	 * These viewports also require visible UVLightCard actors to render.
	 */
	static bool IsShouldUseUVLightcard(FDisplayClusterViewportManager& InViewportManager, const FDisplayClusterConfigurationICVFX_LightcardSettings& InLightcardSettings);

#if WITH_EDITOR
	static TArray<FDisplayClusterViewport*> PreviewGetRenderedInCameraViewports(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, bool bGetChromakey = false);
	static void PreviewReuseInnerFrustumViewportWithinClusterNodes_Editor(FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
	static void PreviewReuseChromakeyViewportWithinClusterNodes_Editor(FDisplayClusterViewport& InChromakeyViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
#endif

	static void UpdateCameraViewportSettings(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
	static void UpdateChromakeyViewportSettings(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
	static void UpdateLightcardViewportSetting(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor);

	static bool IsCameraUsed(UDisplayClusterICVFXCameraComponent& InCameraComponent);
	static bool GetCameraContext(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, FCameraContext_ICVFX& OutCameraContext);

	static FDisplayClusterShaderParameters_ICVFX::FCameraSettings GetShaderParametersCameraSettings(const FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);
	
	static void UpdateCameraSettings_Chromakey(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_ChromakeySettings& InChromakeySettings, FDisplayClusterViewport* InChromakeyViewport);
	static void UpdateCameraSettings_ChromakeyMarkers(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_ChromakeyMarkers& InChromakeyMarkers);

	static void UpdateCameraCustomFrustum(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_CameraCustomFrustum& InCameraCustomFrustum);
	static void UpdateCameraViewportBufferRatio(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings);

	static FDisplayClusterViewport* ImplFindViewport(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId);
private:
	static FDisplayClusterViewport* ImplCreateViewport(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
};
