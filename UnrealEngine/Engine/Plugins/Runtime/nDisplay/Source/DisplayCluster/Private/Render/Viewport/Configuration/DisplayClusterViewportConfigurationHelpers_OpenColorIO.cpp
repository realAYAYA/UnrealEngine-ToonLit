// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationHelpers_OpenColorIO.h"
#include "DisplayClusterViewportConfiguration.h"

#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterSceneViewExtensions.h"
#include "DisplayClusterRootActor.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "OpenColorIODisplayExtension.h"

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ImplUpdateOuterViewportOCIO(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	if (StageSettings.bUseOverallClusterOCIOConfiguration)
	{
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : StageSettings.PerViewportOCIOProfiles)
		{
			if (OCIOProfileIt.bIsEnabled)
			{
				for (const FString& ViewportNameIt : OCIOProfileIt.ApplyOCIOToObjects)
				{
					if (DstViewport.GetId().Compare(ViewportNameIt, ESearchCase::IgnoreCase) == 0)
					{
						// Use per-viewport OCIO overrides
						if (FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ImplUpdate(DstViewport, OCIOProfileIt.OCIOConfiguration))
						{
							return true;
						}

						break;
					}
				}
			}
		}

		// use all viewports OCIO RootActor
		if (StageSettings.AllViewportsOCIOConfiguration.bIsEnabled)
		{
			if (ImplUpdate(DstViewport, StageSettings.AllViewportsOCIOConfiguration.OCIOConfiguration))
			{
				return true;
			}
		}
	}

	// No valid OCIO for ICVX, disable
	ImplDisable(DstViewport);

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateBaseViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& InViewportConfiguration)
{
	// ICVFX logic for OCIO
	return ImplUpdateOuterViewportOCIO(DstViewport, RootActor);
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateLightcardViewport(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_LightcardSettings& LightcardSettings = StageSettings.Lightcard;

	// First try use global OCIO from stage settings
	if (LightcardSettings.bEnableOuterViewportOCIO)
	{
		return  ImplUpdateOuterViewportOCIO(DstViewport, RootActor);
	}

	// No OCIO defined for this lightcard viewport, disabled
	ImplDisable(DstViewport);
	
	return false;
}

#if WITH_EDITOR
bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::IsInnerFrustumViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	if (CameraSettings.AllNodesOCIOConfiguration.bIsEnabled)
	{
		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : CameraSettings.PerNodeOCIOProfiles)
		{
			if (OCIOProfileIt.bIsEnabled)
			{
				const FString* CustomNode1 = OCIOProfileIt.ApplyOCIOToObjects.FindByPredicate([ClusterNodeId = InViewport1.GetClusterNodeId()](const FString& InClusterNodeId)
				{
					return ClusterNodeId.Equals(InClusterNodeId, ESearchCase::IgnoreCase);
				});

				const FString* CustomNode2 = OCIOProfileIt.ApplyOCIOToObjects.FindByPredicate([ClusterNodeId = InViewport2.GetClusterNodeId()](const FString& InClusterNodeId)
				{
					return ClusterNodeId.Equals(InClusterNodeId, ESearchCase::IgnoreCase);
				});

				if (CustomNode1 && CustomNode2)
				{
					// equal custom settings
					return true;
				}

				if (CustomNode1 || CustomNode2)
				{
					// one of node has custom settings
					return false;
				}
			}
		}
	}

	return true;
}
#endif

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateICVFXCameraViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	if (CameraSettings.AllNodesOCIOConfiguration.bIsEnabled)
	{
		const FDisplayClusterRenderFrameSettings& RenderFrameSettings = DstViewport.Owner.GetRenderFrameSettings();

		const FString& ClusterNodeId = DstViewport.GetClusterNodeId();
		check(!ClusterNodeId.IsEmpty());

		for (const FDisplayClusterConfigurationOCIOProfile& OCIOProfileIt : CameraSettings.PerNodeOCIOProfiles)
		{
			if (OCIOProfileIt.bIsEnabled)
			{
				for (const FString& ClusterNodeIt : OCIOProfileIt.ApplyOCIOToObjects)
				{
					if (ClusterNodeId.Compare(ClusterNodeIt, ESearchCase::IgnoreCase) == 0)
					{
						// Use cluster node OCIO
						if (FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ImplUpdate(DstViewport, OCIOProfileIt.OCIOConfiguration))
						{
							return true;
						}

						break;
					}
				}
			}
		}

		// cluster node OCIO override not found, use all nodes configuration
		if (ImplUpdate(DstViewport, CameraSettings.AllNodesOCIOConfiguration.OCIOConfiguration))
		{
			return true;
		}
	}

	// No OCIO defined for this camera, disabled
	ImplDisable(DstViewport);

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ImplUpdate(FDisplayClusterViewport& DstViewport, const FOpenColorIODisplayConfiguration& InConfiguration)
{
	if (InConfiguration.bIsEnabled && InConfiguration.ColorConfiguration.IsValid())
	{
		// Create/Update OCIO:
		if (DstViewport.OpenColorIODisplayExtension.IsValid() == false)
		{
			DstViewport.OpenColorIODisplayExtension = FSceneViewExtensions::NewExtension<FOpenColorIODisplayExtension>(nullptr);
			if (DstViewport.OpenColorIODisplayExtension.IsValid())
			{
				// assign active func
				DstViewport.OpenColorIODisplayExtension->IsActiveThisFrameFunctions.Reset(1);
				DstViewport.OpenColorIODisplayExtension->IsActiveThisFrameFunctions.Add(DstViewport.GetSceneViewExtensionIsActiveFunctor());
			}
		}

		if (DstViewport.OpenColorIODisplayExtension.IsValid())
		{
			// Update configuration
			DstViewport.OpenColorIODisplayExtension->SetDisplayConfiguration(InConfiguration);
		}

		return true;
	}

	return false;
}

void FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ImplDisable(FDisplayClusterViewport& DstViewport)
{
	// Remove OICO ref
	DstViewport.OpenColorIODisplayExtension.Reset();
}

