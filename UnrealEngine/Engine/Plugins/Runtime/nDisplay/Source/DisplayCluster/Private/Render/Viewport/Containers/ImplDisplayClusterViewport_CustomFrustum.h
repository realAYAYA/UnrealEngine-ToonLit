// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterViewport;

enum class EDisplayClusterViewport_CustomFrustumMode: uint8
{
	Percent,
	Pixels,
	Disabled,
};

struct FImplDisplayClusterViewport_CustomFrustumSettings
{
	EDisplayClusterViewport_CustomFrustumMode Mode = EDisplayClusterViewport_CustomFrustumMode::Disabled;
	
	bool bAdaptResolution;

	float Left = 0;
	float Right = 0;
	float Top = 0;
	float Bottom = 0;
};

struct FDisplayClusterViewport_CustomFrustumSettings
{
	struct FCustomFrustumPercent
	{
		float Left = 0;
		float Right = 0;
		float Top = 0;
		float Bottom = 0;
	};

	struct FCustomFrustumPixels
	{
		inline FIntRect GetInnerRect(const FIntRect& InRect) const
		{
			const FIntPoint InnerSize = InRect.Size() - Size();
			const FIntPoint InnerPos = FIntPoint(Left, Top);

			return FIntRect(InnerPos, InnerPos + InnerSize);
		}

		inline FIntPoint Size() const
		{
			return FIntPoint(Left + Right, Top + Bottom);
		}

		int32 Left = 0;
		int32 Right = 0;
		int32 Top = 0;
		int32 Bottom = 0;
	};

	bool bIsEnabled = false;

	// CustomFrustum sides in percent
	FCustomFrustumPercent CustomFrustumPercent;

	// CustomFrustum sides in pixels
	FCustomFrustumPixels  CustomFrustumPixels;
};

class FImplDisplayClusterViewport_CustomFrustum
{
public:
	void Update(FDisplayClusterViewport& Viewport, FIntRect& InOutRenderTargetRect);
	bool UpdateProjectionAngles(float& InOutLeft, float& InOutRight, float& InOutTop, float& InOutBottom);

	bool IsEnabled() const
	{
		return RuntimeSettings.bIsEnabled;
	}

	void Set(const FImplDisplayClusterViewport_CustomFrustumSettings& InSettings)
	{
		CustomFrustumSettings = InSettings;
	}

	const FDisplayClusterViewport_CustomFrustumSettings& GetRuntimeSettingsRef() const
	{
		return RuntimeSettings;
	}

	void ResetConfiguration()
	{
		CustomFrustumSettings = FImplDisplayClusterViewport_CustomFrustumSettings();
		RuntimeSettings = FDisplayClusterViewport_CustomFrustumSettings();
	}

	void Disable()
	{
		CustomFrustumSettings.Mode = EDisplayClusterViewport_CustomFrustumMode::Percent;
		RuntimeSettings.bIsEnabled = false;
	}

private:
	// Settings from configuration
	FImplDisplayClusterViewport_CustomFrustumSettings CustomFrustumSettings;

	// Runtime settings
	FDisplayClusterViewport_CustomFrustumSettings RuntimeSettings;
};

