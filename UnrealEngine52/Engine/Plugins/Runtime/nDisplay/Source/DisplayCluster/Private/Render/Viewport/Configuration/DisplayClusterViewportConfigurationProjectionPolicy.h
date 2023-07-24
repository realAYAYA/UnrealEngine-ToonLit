// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterViewportManager;
class FDisplayClusterViewport;

class UDisplayClusterConfigurationData;
class UDisplayClusterConfigurationViewport;

class UCameraComponent;
class UDisplayClusterICVFXCameraComponent;

class ADisplayClusterRootActor;

struct FDisplayClusterViewportConfigurationProjectionPolicy
{
public:
	FDisplayClusterViewportConfigurationProjectionPolicy(FDisplayClusterViewportManager& InViewportManager, ADisplayClusterRootActor& InRootActor, const UDisplayClusterConfigurationData& InConfigurationData)
		: RootActor(InRootActor)
		, ViewportManager(InViewportManager)
		, ConfigurationData(InConfigurationData)
	{}

public:
	void Update();

private:
	bool UpdateCameraPolicy(FDisplayClusterViewport& DstViewport);
	bool UpdateCameraPolicy_Base(FDisplayClusterViewport& DstViewport, UCameraComponent* const InCameraComponent, float FOVMultiplier);
	bool UpdateCameraPolicy_ICVFX(FDisplayClusterViewport& DstViewport, UDisplayClusterICVFXCameraComponent* const InICVFXCameraComponent);

private:
	ADisplayClusterRootActor& RootActor;
	FDisplayClusterViewportManager& ViewportManager;
	const UDisplayClusterConfigurationData& ConfigurationData;
};

