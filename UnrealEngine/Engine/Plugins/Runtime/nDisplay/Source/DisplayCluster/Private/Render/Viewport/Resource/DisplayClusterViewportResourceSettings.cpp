// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Resource/DisplayClusterViewportResourceSettings.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

/////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportResource
/////////////////////////////////////////////////////////////////////
FDisplayClusterViewportResourceSettings::FDisplayClusterViewportResourceSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FViewport* InViewport)
	: ClusterNodeId(InRenderFrameSettings.ClusterNodeId)
{
	if (InViewport && InViewport->GetRenderTargetTexture())
	{
		FRHITexture2D* ViewportTexture = InViewport->GetRenderTargetTexture();
		Format = ViewportTexture->GetFormat();

		if (EnumHasAnyFlags(ViewportTexture->GetFlags(), TexCreate_SRGB))
		{
			EnumAddFlags(ResourceFlags, EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);
		}

		DisplayGamma = InViewport->GetDisplayGamma();
	}
	else
	{
		// Use default settings:
		Format = FDisplayClusterViewportHelpers::GetDefaultPixelFormat();
		DisplayGamma = 2.2f;

		// Always use srgb for preview rendering
		EnumAddFlags(ResourceFlags, EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);

		if (InRenderFrameSettings.ShouldUseLinearGamma())
		{
				DisplayGamma = 1.f;
		}

		if(InRenderFrameSettings.IsPreviewRendering())
		{
			// for preview rendering use custom pixel format
			Format = FDisplayClusterViewportHelpers::GetPreviewDefaultPixelFormat();
		}
	}
}

FDisplayClusterViewportResourceSettings::FDisplayClusterViewportResourceSettings(const FDisplayClusterViewportResourceSettings& InBaseSettings, const FString InViewportId, const FIntPoint& InSize, const EPixelFormat InFormat, const EDisplayClusterViewportResourceSettingsFlags InResourceFlags, const int32 InNumMips)
	: ClusterNodeId(InBaseSettings.ClusterNodeId)
	, ViewportId(InViewportId)
	, Size(InSize)
	, Format((InFormat == PF_Unknown) ? InBaseSettings.Format : InFormat)
	, DisplayGamma(InBaseSettings.DisplayGamma)
	, NumMips(InNumMips)
	, ResourceFlags(InBaseSettings.ResourceFlags)
{
	EnumAddFlags(ResourceFlags, InResourceFlags);
}
