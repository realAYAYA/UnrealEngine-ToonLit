// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "HAL/IConsoleManager.h"


////////////////////////////////////////////////////////////////////////////////
// Experimental feature: to be approved after testing
int32 GDisplayClusterPreviewEnableReuseViewportInCluster = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableReuseViewportInCluster(
	TEXT("DC.Preview.EnableReuseViewportInCluster"),
	GDisplayClusterPreviewEnableReuseViewportInCluster,
	TEXT("Experimental feature (0 == disabled, 1 == enabled)"),
	ECVF_RenderThreadSafe
);

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterRenderFrameSettings
////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterRenderFrameSettings::IsPreviewFreezeRender() const
{
	return IsPreviewRendering() && PreviewSettings.bFreezePreviewRender;
}
const FIntPoint* FDisplayClusterRenderFrameSettings::GetPreviewMultiGPURendering() const
{
	if (IsPreviewRendering() && PreviewMultiGPURendering.IsSet())
	{
		const FIntPoint& GPURange = PreviewMultiGPURendering.GetValue();
		if (GPURange.X <= GPURange.Y)
		{
			return &GPURange;
		}
	}

	return nullptr;
}

FIntPoint FDisplayClusterRenderFrameSettings::GetDesiredRTTSize(const FIntPoint& InSize) const
{
	FVector2D NewSize = GetDesiredRTTSize(FVector2D(InSize.X, InSize.Y));

	return FIntPoint(NewSize.X, NewSize.Y);
}

FVector2D FDisplayClusterRenderFrameSettings::GetDesiredFrameMult() const
{
	const float BaseMult = IsPreviewRendering() ? FMath::Clamp(PreviewSettings.PreviewRenderTargetRatioMult, 0.f, 1.f) : 1.f;

	return GetDesiredRTTSize(FVector2D(BaseMult, BaseMult));
}

bool FDisplayClusterRenderFrameSettings::CanReuseViewportWithinClusterNodes() const
{
	if (GDisplayClusterPreviewEnableReuseViewportInCluster > 0)
	{
		if (IsPreviewRendering())
		{
			return true;
		}
	}

	return false;
}

int32 FDisplayClusterRenderFrameSettings::GetViewportTextureMaxSize() const
{
	if (IsPreviewRendering())
	{
		// Special constraints for texture when rendering previews
		return PreviewSettings.PreviewMaxTextureDimension;
	}

	return -1;
}

bool FDisplayClusterRenderFrameSettings::ShouldUseLinearGamma() const
{
	if (IsPreviewRendering())
	{
		return !PreviewSettings.bPreviewEnablePostProcess;
	}

	return false;
}

bool FDisplayClusterRenderFrameSettings::IsPostProcessDisabled() const
{
	if (IsPreviewRendering())
	{
		return !PreviewSettings.bPreviewEnablePostProcess;
	}

	return false;
}

bool FDisplayClusterRenderFrameSettings::IsPreviewRendering() const
{
	if (RenderMode == EDisplayClusterRenderFrameMode::PreviewInScene
		|| RenderMode == EDisplayClusterRenderFrameMode::PreviewProxyHitInScene)
	{
		return PreviewSettings.bPreviewEnable;
	}

	return false;
}

bool FDisplayClusterRenderFrameSettings::IsTechvisEnabled() const
{
	// Don't use Techvis to render ProxyHit
	return RenderMode == EDisplayClusterRenderFrameMode::PreviewInScene
		&& PreviewSettings.bEnablePreviewTechvis;
}

bool FDisplayClusterRenderFrameSettings::IsPreviewInGameEnabled() const
{
	// Don't use Techvis to render ProxyHit
	return RenderMode == EDisplayClusterRenderFrameMode::PreviewInScene
		&& PreviewSettings.bPreviewInGameEnable;
}

FVector2D FDisplayClusterRenderFrameSettings::GetDesiredRTTSize(const FVector2D& InSize) const
{
	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::SideBySide:
	case EDisplayClusterRenderFrameMode::PIE_SideBySide:
		return FVector2D(InSize.X * 0.5f, InSize.Y);

	case EDisplayClusterRenderFrameMode::TopBottom:
	case EDisplayClusterRenderFrameMode::PIE_TopBottom:
		return FVector2D(InSize.X, InSize.Y * 0.5f);

	default:
		break;
	}

	return InSize;
}

int32 FDisplayClusterRenderFrameSettings::GetViewPerViewportAmount() const
{
	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::Stereo:
	case EDisplayClusterRenderFrameMode::SideBySide:
	case EDisplayClusterRenderFrameMode::TopBottom:
	case EDisplayClusterRenderFrameMode::PIE_SideBySide:
	case EDisplayClusterRenderFrameMode::PIE_TopBottom:
		return 2;

	default:
		break;
	}

	return 1;
}

bool FDisplayClusterRenderFrameSettings::ShouldUseOutputFrameTargetableResources() const
{
	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
	case EDisplayClusterRenderFrameMode::PreviewProxyHitInScene:
		// Preview uses its own RTTs for each viewport.
		return false;

	default:
		break;
	}

	return bShouldUseOutputTargetableResources;
}

bool FDisplayClusterRenderFrameSettings::ShouldUseStereoRenderingOnMonoscopicDisplay() const
{
	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::SideBySide:
	case EDisplayClusterRenderFrameMode::TopBottom:
	case EDisplayClusterRenderFrameMode::PIE_SideBySide:
	case EDisplayClusterRenderFrameMode::PIE_TopBottom:
		return true;

	default:
		break;
	}

	return false;
}
