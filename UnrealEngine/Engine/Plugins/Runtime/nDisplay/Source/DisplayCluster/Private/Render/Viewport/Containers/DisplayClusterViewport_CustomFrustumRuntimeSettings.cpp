// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewport_CustomFrustumRuntimeSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_CustomFrustumSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "HAL/IConsoleManager.h"

int32 GDisplayClusterRenderCustomFrustumEnable = 1;
static FAutoConsoleVariableRef CVarDisplayClusterRenderCustomFrustumEnable(
	TEXT("nDisplay.render.custom_frustum.enable"),
	GDisplayClusterRenderCustomFrustumEnable,
	TEXT("Enable custom frustum feature.\n")
	TEXT(" 0 - to disable.\n"),
	ECVF_Default
);

int32 GDisplayClusterRenderCustomFrustumMaxValue = 50;
static FAutoConsoleVariableRef CVarDisplayClusterRenderCustomFrustumMaxValue(
	TEXT("nDisplay.render.custom_frustum.max_percent"),
	GDisplayClusterRenderCustomFrustumMaxValue,
	TEXT("Max percent for custom frustum (default 50).\n"),
	ECVF_Default
);

namespace UE::DisplayCluster::Viewport::CustomFrustumHelpers
{
	/** Clamp percent for custom frustum settings. */
	static inline double ClampPercent(double InValue)
	{
		const double MaxCustomFrustumValue = double(GDisplayClusterRenderCustomFrustumMaxValue) / 100;

		return FMath::Clamp(InValue, -MaxCustomFrustumValue, MaxCustomFrustumValue);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport_CustomFrustumRuntimeSettings
///////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterViewport_CustomFrustumRuntimeSettings::UpdateProjectionAngles(
	const FDisplayClusterViewport_CustomFrustumRuntimeSettings& InRuntimeSettings,
	const FIntPoint& InRenderTargetSize,
	double& InOutLeft,
	double& InOutRight,
	double& InOutTop,
	double& InOutBottom)
{
	if (InRuntimeSettings.bIsEnabled)
	{
		const double Horizontal = InOutRight - InOutLeft;
		const double Vertical = InOutTop - InOutBottom;

		// Use the inner region of the texture as the base of the frustum.
		const FIntPoint InnerSize = InRenderTargetSize - InRuntimeSettings.CustomFrustumPixels.Size();

		InOutLeft -= Horizontal * InRuntimeSettings.CustomFrustumPixels.Left / InnerSize.X;
		InOutRight += Horizontal * InRuntimeSettings.CustomFrustumPixels.Right / InnerSize.X;
		InOutBottom -= Vertical * InRuntimeSettings.CustomFrustumPixels.Bottom / InnerSize.Y;
		InOutTop += Vertical * InRuntimeSettings.CustomFrustumPixels.Top / InnerSize.Y;

		return true;
	}

	return false;
}

void FDisplayClusterViewport_CustomFrustumRuntimeSettings::UpdateCustomFrustumSettings(
	const FString& InViewportId,
	const FDisplayClusterViewport_CustomFrustumSettings& InCustomFrustumSettings,
	FDisplayClusterViewport_CustomFrustumRuntimeSettings& InOutRuntimeSettings,
	FIntRect& InOutRenderTargetRect)
{
	using namespace UE::DisplayCluster::Viewport;

	const FIntPoint Size = InOutRenderTargetRect.Size();

	// Disable viewport CustomFrustum feature
	if (!GDisplayClusterRenderCustomFrustumEnable || !InCustomFrustumSettings.bEnabled)
	{
		return;
	}

	switch (InCustomFrustumSettings.Unit)
	{
	case EDisplayClusterViewport_FrustumUnit::Percent:
	{
		InOutRuntimeSettings.bIsEnabled = true;

		InOutRuntimeSettings.CustomFrustumPercent.Left   = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Left);
		InOutRuntimeSettings.CustomFrustumPercent.Right  = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Right);
		InOutRuntimeSettings.CustomFrustumPercent.Top    = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Top);
		InOutRuntimeSettings.CustomFrustumPercent.Bottom = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Bottom);
		break;
	}

	case EDisplayClusterViewport_FrustumUnit::Pixels:
	{
		InOutRuntimeSettings.bIsEnabled = true;

		InOutRuntimeSettings.CustomFrustumPercent.Left   = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Left   / Size.X);
		InOutRuntimeSettings.CustomFrustumPercent.Right  = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Right  / Size.X);
		InOutRuntimeSettings.CustomFrustumPercent.Top    = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Top    / Size.Y);
		InOutRuntimeSettings.CustomFrustumPercent.Bottom = CustomFrustumHelpers::ClampPercent(InCustomFrustumSettings.Bottom / Size.Y);

		break;
	}

	default:
		return;
	}

	// Calc pixels from percent
	InOutRuntimeSettings.CustomFrustumPixels.Left   = FMath::RoundToInt(Size.X * InOutRuntimeSettings.CustomFrustumPercent.Left);
	InOutRuntimeSettings.CustomFrustumPixels.Right  = FMath::RoundToInt(Size.X * InOutRuntimeSettings.CustomFrustumPercent.Right);
	InOutRuntimeSettings.CustomFrustumPixels.Top    = FMath::RoundToInt(Size.Y * InOutRuntimeSettings.CustomFrustumPercent.Top);
	InOutRuntimeSettings.CustomFrustumPixels.Bottom = FMath::RoundToInt(Size.Y * InOutRuntimeSettings.CustomFrustumPercent.Bottom);

	// Update RTT size for CustomFrustum when we need to scale target resolution
	if (InCustomFrustumSettings.bAdaptResolution)
	{
		const FIntPoint CustomFrustumSize = Size + InOutRuntimeSettings.CustomFrustumPixels.Size();
		const FIntPoint ValidCustomFrustumSize = FDisplayClusterViewportHelpers::GetValidViewportRect(FIntRect(FIntPoint(0, 0), CustomFrustumSize), InViewportId, TEXT("CustomFrustum")).Size();

		InOutRenderTargetRect.Max = ValidCustomFrustumSize;
	}
}
