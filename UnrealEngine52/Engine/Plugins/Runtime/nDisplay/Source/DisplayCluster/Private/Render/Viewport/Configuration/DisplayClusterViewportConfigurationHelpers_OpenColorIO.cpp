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

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationHelpers_OpenColorIO
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateBaseViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& InViewportConfiguration)
{
	if (!EnumHasAnyFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		if (const FOpenColorIOColorConversionSettings* OCIOConfiguration = RootActor.GetStageSettings().FindViewportOCIOConfiguration(DstViewport.GetId()))
		{
			ApplyOCIOConfiguration(DstViewport, *OCIOConfiguration);

			return true;
		}

		DisableOCIOConfiguration(DstViewport);
	}

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateLightcardViewport(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	if (EnumHasAnyFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		if (const FOpenColorIOColorConversionSettings* OCIOConfiguration = RootActor.GetStageSettings().FindLightcardOCIOConfiguration(BaseViewport.GetId()))
		{
			ApplyOCIOConfiguration(DstViewport, *OCIOConfiguration);

			return true;
		}

		// No OCIO defined for this lightcard viewport, disabled
		DisableOCIOConfiguration(DstViewport);
	}

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateCameraViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	if (EnumHasAllFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera))
	{
		if (const FOpenColorIOColorConversionSettings* OCIOConfiguration = InCameraComponent.GetCameraSettingsICVFX().FindInnerFrustumOCIOConfiguration(DstViewport.GetClusterNodeId()))
		{
			ApplyOCIOConfiguration(DstViewport, *OCIOConfiguration);

			return true;
		}

		DisableOCIOConfiguration(DstViewport);
	}

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateChromakeyViewport(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	if (EnumHasAllFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Chromakey))
	{
		if (const FOpenColorIOColorConversionSettings* OCIOConfiguration = InCameraComponent.GetCameraSettingsICVFX().FindChromakeyOCIOConfiguration(DstViewport.GetClusterNodeId()))
		{
			ApplyOCIOConfiguration(DstViewport, *OCIOConfiguration);

			return true;
		}

		DisableOCIOConfiguration(DstViewport);
	}

	return false;
}

void FDisplayClusterViewportConfigurationHelpers_OpenColorIO::ApplyOCIOConfiguration(FDisplayClusterViewport& DstViewport, const FOpenColorIOColorConversionSettings& InConversionSettings)
{
	// Check OCIO before apply
	if(!InConversionSettings.IsValid())
	{
		DisableOCIOConfiguration(DstViewport);

		return;
	}

	if (DstViewport.OpenColorIO.IsValid() && DstViewport.OpenColorIO->IsConversionSettingsEqual(InConversionSettings))
	{
		// Already assigned
		return;
	}

	DstViewport.OpenColorIO.Reset();
	DstViewport.OpenColorIO = MakeShared<FDisplayClusterViewport_OpenColorIO>(InConversionSettings);
}

void FDisplayClusterViewportConfigurationHelpers_OpenColorIO::DisableOCIOConfiguration(FDisplayClusterViewport& DstViewport)
{
	// Remove OICO ref
	DstViewport.OpenColorIO.Reset();
}

#if WITH_EDITOR
bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::IsInnerFrustumViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	return InCameraComponent.GetCameraSettingsICVFX().IsInnerFrustumViewportSettingsEqual_Editor(InViewport1.GetClusterNodeId(), InViewport2.GetClusterNodeId());
}

bool FDisplayClusterViewportConfigurationHelpers_OpenColorIO::IsChromakeyViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	return InCameraComponent.GetCameraSettingsICVFX().IsChromakeyViewportSettingsEqual_Editor(InViewport1.GetClusterNodeId(), InViewport2.GetClusterNodeId());
}
#endif
