// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

class FDisplayClusterViewport;

enum class EDisplayClusterViewport_OverscanMode: uint8
{
	None,
	Pixels,
	Percent
};

struct FImplDisplayClusterViewport_OverscanSettings
{
	bool bEnabled = false;
	bool bOversize = false;

	EDisplayClusterViewport_OverscanMode Mode = EDisplayClusterViewport_OverscanMode::None;

	float Left = 0;
	float Right = 0;
	float Top = 0;
	float Bottom = 0;
};

class FImplDisplayClusterViewport_Overscan
{
public:
	void Update(FDisplayClusterViewport& Viewport, FIntRect& InOutRenderTargetRect);
	bool UpdateProjectionAngles(float& InOutLeft, float& InOutRight, float& InOutTop, float& InOutBottom);

	bool IsEnabled() const
	{
		return RuntimeSettings.bIsEnabled;
	}

	void Set(const FImplDisplayClusterViewport_OverscanSettings& InSettings)
	{
		OverscanSettings = InSettings;
	}

	const FDisplayClusterViewport_OverscanSettings& Get() const
	{
		return RuntimeSettings;
	}

	void ResetConfiguration()
	{
		OverscanSettings = FImplDisplayClusterViewport_OverscanSettings();
		RuntimeSettings = FDisplayClusterViewport_OverscanSettings();
	}

	void Disable()
	{
		OverscanSettings.Mode = EDisplayClusterViewport_OverscanMode::None;
		RuntimeSettings.bIsEnabled = false;
	}

private:
	// Settings from configuration
	FImplDisplayClusterViewport_OverscanSettings OverscanSettings;

	// Runtime settings
	FDisplayClusterViewport_OverscanSettings RuntimeSettings;
};

