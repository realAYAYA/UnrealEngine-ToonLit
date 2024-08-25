// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanRuntimeSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "HAL/IConsoleManager.h"

int32 GDisplayClusterRenderOverscanEnable = 1;
static FAutoConsoleVariableRef CVarDisplayClusterRenderOverscanEnable(
	TEXT("nDisplay.render.overscan.enable"),
	GDisplayClusterRenderOverscanEnable,
	TEXT("Enable overscan feature.\n")
	TEXT(" 0 - to disable.\n"),
	ECVF_Default
);

int32 GDisplayClusterRenderOverscanMaxValue = 50;
static FAutoConsoleVariableRef CVarDisplayClusterRenderOverscanMaxValue(
	TEXT("nDisplay.render.overscan.max_percent"),
	GDisplayClusterRenderOverscanMaxValue,
	TEXT("Max percent for overscan (default 50).\n"),
	ECVF_Default
);

namespace UE::DisplayCluster::Viewport::OverscanHelpers
{
	/** Clamp percent for overscan settings. */
	static inline double ClampPercent(double InValue)
	{
		const double MaxCustomFrustumValue = double(GDisplayClusterRenderOverscanMaxValue) / 100;

		return FMath::Clamp(InValue, -MaxCustomFrustumValue, MaxCustomFrustumValue);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport_OverscanRuntimeSettings
///////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterViewport_OverscanRuntimeSettings::UpdateProjectionAngles(
	const FDisplayClusterViewport_OverscanRuntimeSettings& InOverscanRuntimeSettings,
	const FIntPoint& InRenderTargetSize,
	double& InOutLeft,
	double& InOutRight,
	double& InOutTop,
	double& InOutBottom)
{
	if (InOverscanRuntimeSettings.bIsEnabled)
	{
		double Horizontal = InOutRight - InOutLeft;
		double Vertical = InOutTop - InOutBottom;

		// Use the inner region of the texture as the base of the frustum.
		const FIntPoint InnerSize = InRenderTargetSize - InOverscanRuntimeSettings.OverscanPixels.Size();

		InOutLeft   -= Horizontal * InOverscanRuntimeSettings.OverscanPixels.Left / InnerSize.X;
		InOutRight  += Horizontal * InOverscanRuntimeSettings.OverscanPixels.Right / InnerSize.X;
		InOutBottom -= Vertical * InOverscanRuntimeSettings.OverscanPixels.Bottom / InnerSize.Y;
		InOutTop    += Vertical * InOverscanRuntimeSettings.OverscanPixels.Top / InnerSize.Y;

		return true;
	}

	return false;
}

void FDisplayClusterViewport_OverscanRuntimeSettings::UpdateOverscanSettings(
	const FString& InViewportId,
	const FDisplayClusterViewport_OverscanSettings& InOverscanSettings,
	FDisplayClusterViewport_OverscanRuntimeSettings& InOutOverscanRuntimeSettings,
	FIntRect& InOutRenderTargetRect)
{
	using namespace UE::DisplayCluster::Viewport;

	const FIntPoint Size = InOutRenderTargetRect.Size();

	// Disable viewport overscan feature
	if (!GDisplayClusterRenderOverscanEnable || !InOverscanSettings.bEnabled)
	{
		return;
	}

	switch (InOverscanSettings.Unit)
	{
	case EDisplayClusterViewport_FrustumUnit::Percent:
	{
		InOutOverscanRuntimeSettings.bIsEnabled = true;

		InOutOverscanRuntimeSettings.OverscanPercent.Left   = OverscanHelpers::ClampPercent(InOverscanSettings.Left);
		InOutOverscanRuntimeSettings.OverscanPercent.Right  = OverscanHelpers::ClampPercent(InOverscanSettings.Right);
		InOutOverscanRuntimeSettings.OverscanPercent.Top    = OverscanHelpers::ClampPercent(InOverscanSettings.Top);
		InOutOverscanRuntimeSettings.OverscanPercent.Bottom = OverscanHelpers::ClampPercent(InOverscanSettings.Bottom);
		break;
	}

	case EDisplayClusterViewport_FrustumUnit::Pixels:
	{
		InOutOverscanRuntimeSettings.bIsEnabled = true;

		InOutOverscanRuntimeSettings.OverscanPercent.Left   = OverscanHelpers::ClampPercent(InOverscanSettings.Left   / Size.X);
		InOutOverscanRuntimeSettings.OverscanPercent.Right  = OverscanHelpers::ClampPercent(InOverscanSettings.Right  / Size.X);
		InOutOverscanRuntimeSettings.OverscanPercent.Top    = OverscanHelpers::ClampPercent(InOverscanSettings.Top    / Size.Y);
		InOutOverscanRuntimeSettings.OverscanPercent.Bottom = OverscanHelpers::ClampPercent(InOverscanSettings.Bottom / Size.Y);

		break;
	}

	default:
		break;
	}

	// Update RTT size for overscan
	if (InOutOverscanRuntimeSettings.bIsEnabled)
	{
		// Calc pixels from percent
		InOutOverscanRuntimeSettings.OverscanPixels.Left   = Size.X * InOutOverscanRuntimeSettings.OverscanPercent.Left;
		InOutOverscanRuntimeSettings.OverscanPixels.Right  = Size.X * InOutOverscanRuntimeSettings.OverscanPercent.Right;
		InOutOverscanRuntimeSettings.OverscanPixels.Top    = Size.Y * InOutOverscanRuntimeSettings.OverscanPercent.Top;
		InOutOverscanRuntimeSettings.OverscanPixels.Bottom = Size.Y * InOutOverscanRuntimeSettings.OverscanPercent.Bottom;

		const FIntPoint OverscanSize = Size + InOutOverscanRuntimeSettings.OverscanPixels.Size();
		const FIntPoint ValidOverscanSize = FDisplayClusterViewportHelpers::GetValidViewportRect(FIntRect(FIntPoint(0, 0), OverscanSize), InViewportId, TEXT("Overscan")).Size();
		
		bool bOversize = InOverscanSettings.bOversize;

		if (OverscanSize != ValidOverscanSize)
		{
			// can't use overscan with extra size, disable oversize
			bOversize = false;
		}

		if (bOversize)
		{
			InOutRenderTargetRect.Max = OverscanSize;
		}
		else
		{
			double scaleX = double(Size.X) / OverscanSize.X;
			double scaleY = double(Size.Y) / OverscanSize.Y;

			InOutOverscanRuntimeSettings.OverscanPixels.Left   *= scaleX;
			InOutOverscanRuntimeSettings.OverscanPixels.Right  *= scaleX;
			InOutOverscanRuntimeSettings.OverscanPixels.Top    *= scaleY;
			InOutOverscanRuntimeSettings.OverscanPixels.Bottom *= scaleY;
		}
	}
}
