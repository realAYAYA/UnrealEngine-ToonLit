// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/ImplDisplayClusterViewport_CustomFrustum.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"
#include "Render/Viewport/DisplayClusterViewport.h"

static TAutoConsoleVariable<int32> CVarDisplayClusterRenderCustomFrustumEnable(
	TEXT("nDisplay.render.custom_frustum.enable"),
	1,
	TEXT("Enable custom frustum feature.\n")
	TEXT(" 0 - to disable.\n")
	TEXT(" 1 - to enable.\n"),
	ECVF_RenderThreadSafe
);

bool FImplDisplayClusterViewport_CustomFrustum::UpdateProjectionAngles(float& InOutLeft, float& InOutRight, float& InOutTop, float& InOutBottom)
{
	if (RuntimeSettings.bIsEnabled)
	{
		const float Horizontal = InOutRight - InOutLeft;
		const float Vertical = InOutTop - InOutBottom;

		InOutLeft   -= Horizontal * RuntimeSettings.CustomFrustumPercent.Left;
		InOutRight  += Horizontal * RuntimeSettings.CustomFrustumPercent.Right;
		InOutBottom -= Vertical * RuntimeSettings.CustomFrustumPercent.Bottom;
		InOutTop    += Vertical * RuntimeSettings.CustomFrustumPercent.Top;

		return true;
	}

	return false;
}

void FImplDisplayClusterViewport_CustomFrustum::Update(FDisplayClusterViewport& Viewport, FIntRect& InOutRenderTargetRect)
{
	// Disable CustomFrustum feature from console
	if ((CustomFrustumSettings.Mode == EDisplayClusterViewport_CustomFrustumMode::Disabled) || (CVarDisplayClusterRenderCustomFrustumEnable.GetValueOnGameThread() == 0))
	{
		return;
	}

	// Disable CustomFrustum feature from viewport settings
	if (Viewport.GetRenderSettings().bDisableCustomFrustumFeature)
	{
		return;
	}

	RuntimeSettings.bIsEnabled = true;
	const FIntPoint Size = InOutRenderTargetRect.Size();

	if (CustomFrustumSettings.Mode == EDisplayClusterViewport_CustomFrustumMode::Percent)
	{
		RuntimeSettings.CustomFrustumPercent.Left = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Left);
		RuntimeSettings.CustomFrustumPercent.Right = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Right);
		RuntimeSettings.CustomFrustumPercent.Top = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Top);
		RuntimeSettings.CustomFrustumPercent.Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Bottom);
	}
	else if (CustomFrustumSettings.Mode == EDisplayClusterViewport_CustomFrustumMode::Pixels)
	{
		RuntimeSettings.CustomFrustumPercent.Left   = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Left   / Size.X);
		RuntimeSettings.CustomFrustumPercent.Right  = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Right  / Size.X);
		RuntimeSettings.CustomFrustumPercent.Top    = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Top    / Size.Y);
		RuntimeSettings.CustomFrustumPercent.Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CustomFrustumSettings.Bottom / Size.Y);
	}

	// Calc pixels from percent
	RuntimeSettings.CustomFrustumPixels.Left   = FMath::RoundToInt(Size.X * RuntimeSettings.CustomFrustumPercent.Left);
	RuntimeSettings.CustomFrustumPixels.Right  = FMath::RoundToInt(Size.X * RuntimeSettings.CustomFrustumPercent.Right);
	RuntimeSettings.CustomFrustumPixels.Top    = FMath::RoundToInt(Size.Y * RuntimeSettings.CustomFrustumPercent.Top);
	RuntimeSettings.CustomFrustumPixels.Bottom = FMath::RoundToInt(Size.Y * RuntimeSettings.CustomFrustumPercent.Bottom);

	// Update RTT size for CustomFrustum when we need to scale target resolution
	if (CustomFrustumSettings.bAdaptResolution)
	{
		const FIntPoint CustomFrustumSize = Size + RuntimeSettings.CustomFrustumPixels.Size();
		const FIntPoint ValidCustomFrustumSize = Viewport.GetValidRect(FIntRect(FIntPoint(0, 0), CustomFrustumSize), TEXT("CustomFrustum")).Size();

		InOutRenderTargetRect.Max = ValidCustomFrustumSize;
	}
}
