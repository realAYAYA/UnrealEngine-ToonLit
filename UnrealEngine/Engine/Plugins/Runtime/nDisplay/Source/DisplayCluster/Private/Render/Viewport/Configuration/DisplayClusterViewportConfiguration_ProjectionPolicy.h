// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

class FDisplayClusterViewport;
class UCameraComponent;
class UDisplayClusterICVFXCameraComponent;

/**
 * A helper class that configure projection policyes for viewports.
 */
struct FDisplayClusterViewportConfiguration_ProjectionPolicy
{
public:
	FDisplayClusterViewportConfiguration_ProjectionPolicy(FDisplayClusterViewportConfiguration& InConfiguration)
		: Configuration(InConfiguration)
	{ }

public:
	/** Special logic to additionally perform some updates within projection policies.
	* Currently, only the advanced logic for the "camera" projection policy is implemented.
	*/
	void Update();

private:
	/** Basic implementation of extended logic for projection policy "camera */
	bool UpdateCameraPolicy(FDisplayClusterViewport& DstViewport);

	/** Implementation of extended logic for a common camera component */
	bool UpdateCameraPolicy_Base(FDisplayClusterViewport& DstViewport, const FString& CameraComponentId);

	/** Implementation of extended logic for a ICVFX camera component */
	bool UpdateCameraPolicy_ICVFX(FDisplayClusterViewport& DstViewport, const FString& CameraComponentId);

private:
	FDisplayClusterViewportConfiguration& Configuration;
};
