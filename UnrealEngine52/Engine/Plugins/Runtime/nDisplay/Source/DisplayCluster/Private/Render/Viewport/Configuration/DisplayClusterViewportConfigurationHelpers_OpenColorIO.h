// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterViewport;
class ADisplayClusterRootActor;

class UDisplayClusterConfigurationViewport;
class UDisplayClusterICVFXCameraComponent;

struct FOpenColorIOColorConversionSettings;

class FDisplayClusterViewportConfigurationHelpers_OpenColorIO
{
public:
	/** Update OCIO for base viewport (Outer).
	 *
	 * @param DstViewport             - Dest viewport object.
	 * @param RootActor               - DCRA object.
	 * @param InViewportConfiguration - Actual viewport configuration.
	 *
	 * @return - true if success.
	 */
	static bool UpdateBaseViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& InViewportConfiguration);

	/** Update OCIO for LightCard viewport.
	 *
	 * @param DstViewport  - Dest LightCard viewport object.
	 * @param BaseViewport - Base (Outer) viewport object.
	 * @param RootActor    - DCRA object.
	 *
	 * @return - true if success.
	 */
	static bool UpdateLightcardViewport(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor);

	/** Update OCIO for InnerFrustum(InCamera) viewport.
	 *
	 * @param DstViewport       - Dest InCamera viewport object
	 * @param RootActor         - DCRA object
	 * @param InCameraComponent - InCamera component with configuration
	 *
	 * @return - true if success
	 */
	static bool UpdateCameraViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);

	/** Update OCIO for Chromakey viewport.
	 *
	 * @param DstViewport       - Dest InCamera viewport object
	 * @param RootActor         - DCRA object
	 * @param InCameraComponent - InCamera component with configuration
	 *
	 * @return - true if success
	 */
	static bool UpdateChromakeyViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent);

#if WITH_EDITOR
	/** Compare InnerFrustum OCIO configuration for input viewports
	 *
	 * @param InViewport1       - Viewport #1
	 * @param InViewport2       - Viewport #2
	 * @param InCameraComponent - InCamera component with configuration
	 *
	 * @return - true if OCIO is equals
	 */
	static bool IsInnerFrustumViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent);

	/** Compare Chromakey OCIO configuration for input viewports
	 *
	 * @param InViewport1       - Viewport #1
	 * @param InViewport2       - Viewport #2
	 * @param InCameraComponent - InCamera component with configuration
	 *
	 * @return - true if OCIO is equals
	 */
	static bool IsChromakeyViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent);
#endif

private:
	/** Apply OCIO to viewport.
	 *
	 * @param DstViewport       - Dest viewport object
	 * @param InConversionSettings   - OCIO conversion settings
	 *
	 * @return - none
	 */
	static void ApplyOCIOConfiguration(FDisplayClusterViewport& DstViewport, const FOpenColorIOColorConversionSettings& InConversionSettings);

	/** Reset OCIO for viewport.
	 *
	 * @param DstViewport       - Dest viewport object
	 *
	 * @return - none
	 */
	static void DisableOCIOConfiguration(FDisplayClusterViewport& DstViewport);
};
