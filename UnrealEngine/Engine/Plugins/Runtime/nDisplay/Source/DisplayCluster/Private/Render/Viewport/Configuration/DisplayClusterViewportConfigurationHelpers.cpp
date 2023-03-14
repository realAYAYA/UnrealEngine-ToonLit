// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterViewportConfigurationHelpers_OpenColorIO.h"
#include "DisplayClusterViewportConfigurationHelpers_Postprocess.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "DisplayClusterRootActor.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_PostRender.h"

#include "IDisplayCluster.h"
#include "Cluster/IDisplayClusterClusterManager.h"

#include "ShaderParameters/DisplayClusterShaderParameters_PostprocessBlur.h"
#include "ShaderParameters/DisplayClusterShaderParameters_GenerateMips.h"
#include "ShaderParameters/DisplayClusterShaderParameters_Override.h"
#include "ShaderParameters/DisplayClusterShaderParameters_ICVFX.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "IDisplayClusterProjection.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "DisplayClusterSceneViewExtensions.h"
#include "OpenColorIODisplayExtension.h"

#include "Misc/DisplayClusterLog.h"

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportStereoMode(FDisplayClusterViewport& DstViewport, const EDisplayClusterConfigurationViewport_StereoMode StereoMode)
{
	switch (StereoMode)
	{
	case EDisplayClusterConfigurationViewport_StereoMode::ForceMono:
		DstViewport.RenderSettings.bForceMono = true;
		break;
	default:
		DstViewport.RenderSettings.bForceMono = false;
		break;
	}
}

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_OverlayRenderSettings(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_OverlayAdvancedRenderSettings& InOverlaySettings)
{
	DstViewport.Owner.SetViewportBufferRatio(DstViewport, InOverlaySettings.BufferRatio);
	DstViewport.RenderSettings.RenderTargetRatio = InOverlaySettings.RenderTargetRatio;

	DstViewport.RenderSettings.GPUIndex = InOverlaySettings.GPUIndex;
	DstViewport.RenderSettings.StereoGPUIndex = InOverlaySettings.StereoGPUIndex;

	UpdateViewportStereoMode(DstViewport, InOverlaySettings.StereoMode);

	DstViewport.RenderSettings.RenderFamilyGroup = InOverlaySettings.RenderFamilyGroup;
};

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Override(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationPostRender_Override& InOverride)
{
	DstViewport.PostRenderSettings.Replace.TextureRHI.SafeRelease();

	if (InOverride.bAllowReplace && InOverride.SourceTexture != nullptr)
	{
		FTextureResource* TextureResource = InOverride.SourceTexture->GetResource();
		if(TextureResource)
		{
			FTextureRHIRef& TextureRHI = TextureResource->TextureRHI;

			if (TextureRHI.IsValid())
			{
				DstViewport.PostRenderSettings.Replace.TextureRHI = TextureRHI;
				FIntVector Size = TextureRHI->GetSizeXYZ();

				DstViewport.PostRenderSettings.Replace.Rect = DstViewport.GetValidRect((InOverride.bShouldUseTextureRegion) ? InOverride.TextureRegion.ToRect() : FIntRect(FIntPoint(0, 0), FIntPoint(Size.X, Size.Y)), TEXT("Configuration Override"));
			}
		}
	}
};

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_PostprocessBlur(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationPostRender_BlurPostprocess& InBlurPostprocess)
{
	switch (InBlurPostprocess.Mode)
	{
	case EDisplayClusterConfiguration_PostRenderBlur::Gaussian:
		DstViewport.PostRenderSettings.PostprocessBlur.Mode = EDisplayClusterShaderParameters_PostprocessBlur::Gaussian;
		break;
	case EDisplayClusterConfiguration_PostRenderBlur::Dilate:
		DstViewport.PostRenderSettings.PostprocessBlur.Mode = EDisplayClusterShaderParameters_PostprocessBlur::Dilate;
		break;
	default:
		DstViewport.PostRenderSettings.PostprocessBlur.Mode = EDisplayClusterShaderParameters_PostprocessBlur::None;
		break;
	}

	DstViewport.PostRenderSettings.PostprocessBlur.KernelRadius = InBlurPostprocess.KernelRadius;
	DstViewport.PostRenderSettings.PostprocessBlur.KernelScale = InBlurPostprocess.KernelScale;
};

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Overscan(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationViewport_Overscan& InOverscan)
{
	FImplDisplayClusterViewport_OverscanSettings OverscanSettings;
	OverscanSettings.bEnabled = InOverscan.bEnabled;
	OverscanSettings.bOversize = InOverscan.bOversize;
	
	if (OverscanSettings.bEnabled)
	{
		switch (InOverscan.Mode)
		{
		case EDisplayClusterConfigurationViewportOverscanMode::Percent:
			OverscanSettings.Mode = EDisplayClusterViewport_OverscanMode::Percent;

			// Scale 0..100% to 0..1 range
			OverscanSettings.Left = .01f * InOverscan.Left;
			OverscanSettings.Right = .01f * InOverscan.Right;
			OverscanSettings.Top = .01f * InOverscan.Top;
			OverscanSettings.Bottom = .01f * InOverscan.Bottom;
			break;

		case EDisplayClusterConfigurationViewportOverscanMode::Pixels:
			OverscanSettings.Mode = EDisplayClusterViewport_OverscanMode::Pixels;

			OverscanSettings.Left = InOverscan.Left;
			OverscanSettings.Right = InOverscan.Right;
			OverscanSettings.Top = InOverscan.Top;
			OverscanSettings.Bottom = InOverscan.Bottom;
			break;

		default:
			break;
		}
	}

	DstViewport.OverscanRendering.Set(OverscanSettings);
};

void FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_GenerateMips(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationPostRender_GenerateMips& InGenerateMips)
{
	if (InGenerateMips.bAutoGenerateMips)
	{
		DstViewport.PostRenderSettings.GenerateMips.bAutoGenerateMips = true;

		DstViewport.PostRenderSettings.GenerateMips.MipsSamplerFilter = InGenerateMips.MipsSamplerFilter;

		DstViewport.PostRenderSettings.GenerateMips.MipsAddressU = InGenerateMips.MipsAddressU;
		DstViewport.PostRenderSettings.GenerateMips.MipsAddressV = InGenerateMips.MipsAddressV;

		DstViewport.PostRenderSettings.GenerateMips.MaxNumMipsLimit = (InGenerateMips.bEnabledMaxNumMips) ? InGenerateMips.MaxNumMips : -1;
	}
	else
	{
		// Disable mips
		DstViewport.PostRenderSettings.GenerateMips.bAutoGenerateMips = false;
	}
}

void FDisplayClusterViewportConfigurationHelpers::UpdateBaseViewportSetting(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& InConfigurationViewport)
{
	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// UDisplayClusterConfigurationViewport
	{
		if (InConfigurationViewport.bAllowRendering == false)
		{
			DstViewport.RenderSettings.bEnable = false;
		}

		DstViewport.RenderSettings.CameraId = InConfigurationViewport.Camera;
		DstViewport.RenderSettings.Rect = DstViewport.GetValidRect(InConfigurationViewport.Region.ToRect(), TEXT("Configuration Region"));

		DstViewport.RenderSettings.GPUIndex = InConfigurationViewport.GPUIndex;
		DstViewport.RenderSettings.OverlapOrder = InConfigurationViewport.OverlapOrder;

		// update viewport remap data
		DstViewport.ViewportRemap.UpdateConfiguration(DstViewport, InConfigurationViewport.ViewportRemap);
	}

	const FDisplayClusterConfigurationViewport_RenderSettings& InRenderSettings = InConfigurationViewport.RenderSettings;

	// OCIO
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateBaseViewport(DstViewport, RootActor, InConfigurationViewport);

	// Additional per-viewport PostProcess
	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateCustomPostProcessSettings(DstViewport, RootActor, InRenderSettings.CustomPostprocess);
	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdatePerViewportPostProcessSettings(DstViewport, RootActor);

	{
		DstViewport.Owner.SetViewportBufferRatio(DstViewport, InRenderSettings.BufferRatio);

		UpdateViewportSetting_Overscan(DstViewport, InRenderSettings.Overscan);

		UpdateViewportSetting_Override(DstViewport, InRenderSettings.Replace);
		UpdateViewportSetting_PostprocessBlur(DstViewport, InRenderSettings.PostprocessBlur);
		UpdateViewportSetting_GenerateMips(DstViewport, InRenderSettings.GenerateMips);

		UpdateViewportStereoMode(DstViewport, InRenderSettings.StereoMode);

		DstViewport.RenderSettings.StereoGPUIndex = InRenderSettings.StereoGPUIndex;
		DstViewport.RenderSettings.RenderTargetRatio = InRenderSettings.RenderTargetRatio;
		DstViewport.RenderSettings.RenderFamilyGroup = InRenderSettings.RenderFamilyGroup;
	}

	// Set media related configuration (runtime only for now)
	if (IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		const FDisplayClusterConfigurationMedia& MediaSettings = InConfigurationViewport.RenderSettings.Media;

		const FString ThisClusterNodeId = IDisplayCluster::Get().GetClusterMgr()->GetNodeId();
		const bool bThisNodeSharesMedia = MediaSettings.IsMediaSharingUsed() && MediaSettings.MediaSharingNode.Equals(ThisClusterNodeId, ESearchCase::IgnoreCase);

		// Don't render the viewport if media input assigned
		DstViewport.RenderSettings.bSkipSceneRenderingButLeaveResourcesAvailable = MediaSettings.IsMediaSharingUsed() ?
			!bThisNodeSharesMedia :
			!!MediaSettings.MediaSource;

		// Mark this viewport is going to be captured by a capture device
		DstViewport.RenderSettings.bIsBeingCaptured = MediaSettings.IsMediaSharingUsed() ?
			bThisNodeSharesMedia :
			!!MediaSettings.MediaOutput;
	}

	// FDisplayClusterConfigurationViewport_ICVFX property:
	{
		EDisplayClusterViewportICVFXFlags& TargetFlags = DstViewport.RenderSettingsICVFX.Flags;

		if (InConfigurationViewport.ICVFX.bAllowICVFX)
		{
			TargetFlags |= ViewportICVFX_Enable;

			EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode CameraRenderMode = InConfigurationViewport.ICVFX.CameraRenderMode;

			const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
			if (InConfigurationViewport.ICVFX.bAllowInnerFrustum == false || StageSettings.bEnableInnerFrustums == false)
			{
				CameraRenderMode = EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::Disabled;
			}

			switch (CameraRenderMode)
			{
				// Disable camera frame render for this viewport
			case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::Disabled:
				TargetFlags |= ViewportICVFX_DisableCamera | ViewportICVFX_DisableChromakey | ViewportICVFX_DisableChromakeyMarkers;
				break;

				// Disable chromakey render for this viewport
			case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::DisableChromakey:
				TargetFlags |= ViewportICVFX_DisableChromakey | ViewportICVFX_DisableChromakeyMarkers;
				break;

				// Disable chromakey markers render for this viewport
			case EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::DisableChromakeyMarkers:
				TargetFlags |= ViewportICVFX_DisableChromakeyMarkers;
				break;
				// Use default rendering rules
			default:
				break;
			}

			switch (InConfigurationViewport.ICVFX.LightcardRenderMode)
			{
				// Render incamera frame over lightcard for this viewport
			case EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Over:
				TargetFlags |= ViewportICVFX_OverrideLightcardMode;
				DstViewport.RenderSettingsICVFX.ICVFX.LightcardMode = EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Over;
				break;

				// Over lightcard over incamera frame  for this viewport
			case EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Under:
				TargetFlags |= ViewportICVFX_OverrideLightcardMode;
				DstViewport.RenderSettingsICVFX.ICVFX.LightcardMode = EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Under;
				break;

			case EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Disabled:
				TargetFlags |= ViewportICVFX_DisableLightcard;
				break;

			default:
				// Use default lightcard mode
				break;
			}
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	bool bNeedUpdateProjectionPolicy = false;

	// Runtime update projection policy
	if (InConfigurationProjectionPolicy)
	{
		if (DstViewport.ProjectionPolicy.IsValid())
		{
			// Current projection policy valid
			bNeedUpdateProjectionPolicy = DstViewport.ProjectionPolicy->IsConfigurationChanged(InConfigurationProjectionPolicy);
		}
		else
		if (DstViewport.UninitializedProjectionPolicy.IsValid())
		{
			// Current projection policy valid
			bNeedUpdateProjectionPolicy = DstViewport.UninitializedProjectionPolicy->IsConfigurationChanged(InConfigurationProjectionPolicy);
		}
	}

	if (bNeedUpdateProjectionPolicy)
	{
		UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Update projection policy for viewport '%s'."), *DstViewport.GetId());

		// Release current projection
		DstViewport.HandleEndScene();
		DstViewport.UninitializedProjectionPolicy.Reset();

		// Create new projection type interface
		DstViewport.UninitializedProjectionPolicy = FDisplayClusterViewportManager::CreateProjectionPolicy(DstViewport.GetId(), InConfigurationProjectionPolicy);
		DstViewport.HandleStartScene();
	}
	else
	{
		if (!DstViewport.ProjectionPolicy.IsValid())
		{
			if (DstViewport.Owner.IsSceneOpened())
			{
				// Try initialize proj policy every tick (mesh deferred load, etc)
				DstViewport.HandleStartScene();
			}
		}
	}
}
