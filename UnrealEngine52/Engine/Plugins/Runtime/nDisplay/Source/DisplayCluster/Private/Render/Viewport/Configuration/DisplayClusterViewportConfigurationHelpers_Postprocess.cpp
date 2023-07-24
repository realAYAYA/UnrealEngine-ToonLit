// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationHelpers_Postprocess.h"
#include "DisplayClusterViewportConfiguration.h"
#include "DisplayClusterConfigurationTypes.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

static void ImplUpdateCustomPostprocess(FDisplayClusterViewport& DstViewport, bool bEnabled, const FDisplayClusterConfigurationViewport_CustomPostprocessSettings& InCustomPostprocess, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass RenderPass)
{
	if (bEnabled)
	{
		DstViewport.CustomPostProcessSettings.AddCustomPostProcess(RenderPass, InCustomPostprocess.PostProcessSettings, InCustomPostprocess.BlendWeight, InCustomPostprocess.bIsOneFrame);
	}
	else
	{
		DstViewport.CustomPostProcessSettings.RemoveCustomPostProcess(RenderPass);
	}
}

static void ImplRemoveCustomPostprocess(FDisplayClusterViewport& DstViewport, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass RenderPass)
{
	DstViewport.CustomPostProcessSettings.RemoveCustomPostProcess(RenderPass);
}


static bool ImplUpdatePerViewportColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationViewport_PerViewportColorGrading PerViewportColorGrading)
{
	// Check and apply the overall cluster post process settings
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	// enable entire cluster only when global settings is on
	bool bUseEntireClusterPostProcess = StageSettings.EntireClusterColorGrading.bEnableEntireClusterColorGrading & PerViewportColorGrading.bIsEntireClusterEnabled;
	
	FDisplayClusterConfigurationViewport_CustomPostprocessSettings FinalPerViewportColorGrading;
	FinalPerViewportColorGrading.bIsEnabled = true;
	FinalPerViewportColorGrading.bIsOneFrame = true;
	FinalPerViewportColorGrading.BlendWeight = 1;

	// blend with entire cluster
	if (bUseEntireClusterPostProcess)
	{
		FinalPerViewportColorGrading.BlendWeight = StageSettings.EntireClusterColorGrading.ColorGradingSettings.BlendWeight;

		FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(FinalPerViewportColorGrading.PostProcessSettings, StageSettings.EntireClusterColorGrading.ColorGradingSettings, PerViewportColorGrading.ColorGradingSettings);
		FinalPerViewportColorGrading.BlendWeight *= PerViewportColorGrading.ColorGradingSettings.BlendWeight;

		ImplUpdateCustomPostprocess(DstViewport, FinalPerViewportColorGrading.bIsEnabled, FinalPerViewportColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		return true;
	}
	else
	{
		// pass color grading without blending with entire cluster
		FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyBlendPostProcessSettings(FinalPerViewportColorGrading.PostProcessSettings, PerViewportColorGrading.ColorGradingSettings);
		FinalPerViewportColorGrading.BlendWeight *= PerViewportColorGrading.ColorGradingSettings.BlendWeight;

		ImplUpdateCustomPostprocess(DstViewport, FinalPerViewportColorGrading.bIsEnabled, FinalPerViewportColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		return true;
	}

	return false;
}

static bool ImplUpdateEntireClusterColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	if (StageSettings.EntireClusterColorGrading.bEnableEntireClusterColorGrading)
	{
		FDisplayClusterConfigurationViewport_CustomPostprocessSettings FinalEntireClusterColorGrading;
		FinalEntireClusterColorGrading.bIsEnabled = true;
		FinalEntireClusterColorGrading.bIsOneFrame = true;
		FinalEntireClusterColorGrading.BlendWeight = 1;

		FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyBlendPostProcessSettings(FinalEntireClusterColorGrading.PostProcessSettings, StageSettings.EntireClusterColorGrading.ColorGradingSettings);
		ImplUpdateCustomPostprocess(DstViewport, FinalEntireClusterColorGrading.bIsEnabled, FinalEntireClusterColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		return true;
	}
	return false;
}

static bool ImplUpdateIncameraPerNodeColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationViewport_AllNodesColorGrading AllNodesColorGrading, const FDisplayClusterConfigurationViewport_PerNodeColorGrading PerNodeColorGrading)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	FDisplayClusterConfigurationViewport_CustomPostprocessSettings FinalPerNodeColorGrading;
	FinalPerNodeColorGrading.bIsEnabled = true;
	FinalPerNodeColorGrading.bIsOneFrame = true;
	FinalPerNodeColorGrading.BlendWeight = 1;

	const bool bIncludeUseEntireClusterPostProcess = StageSettings.EntireClusterColorGrading.bEnableEntireClusterColorGrading & PerNodeColorGrading.bEntireClusterColorGrading;
	const bool bIncludeAllNodesColorGrading = AllNodesColorGrading.bEnableInnerFrustumAllNodesColorGrading & PerNodeColorGrading.bAllNodesColorGrading;

	if (bIncludeUseEntireClusterPostProcess)
	{
		if (bIncludeAllNodesColorGrading)
		{
			// all three options are enabled - cluster + all + node
			FDisplayClusterViewportConfigurationHelpers_Postprocess::PerNodeBlendPostProcessSettings(FinalPerNodeColorGrading.PostProcessSettings, StageSettings.EntireClusterColorGrading.ColorGradingSettings, AllNodesColorGrading.ColorGradingSettings, PerNodeColorGrading.ColorGradingSettings);
			FinalPerNodeColorGrading.BlendWeight *= AllNodesColorGrading.ColorGradingSettings.BlendWeight * PerNodeColorGrading.ColorGradingSettings.BlendWeight;

			ImplUpdateCustomPostprocess(DstViewport, FinalPerNodeColorGrading.bIsEnabled, FinalPerNodeColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		}
		else
		{
			// only cluster + node
			FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(FinalPerNodeColorGrading.PostProcessSettings, StageSettings.EntireClusterColorGrading.ColorGradingSettings, PerNodeColorGrading.ColorGradingSettings);
			FinalPerNodeColorGrading.BlendWeight *= PerNodeColorGrading.ColorGradingSettings.BlendWeight;
			ImplUpdateCustomPostprocess(DstViewport, FinalPerNodeColorGrading.bIsEnabled, FinalPerNodeColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		}

		return true;
	}
	else
	{
		// entire cluster settings disabled, only all nodes cases
		FinalPerNodeColorGrading.BlendWeight = AllNodesColorGrading.ColorGradingSettings.BlendWeight;

		if (bIncludeAllNodesColorGrading)
		{
			// all nodes + node
			FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(FinalPerNodeColorGrading.PostProcessSettings, AllNodesColorGrading.ColorGradingSettings, PerNodeColorGrading.ColorGradingSettings);
			FinalPerNodeColorGrading.BlendWeight *= PerNodeColorGrading.ColorGradingSettings.BlendWeight;
		}
		else
		{
			// node only
			FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyBlendPostProcessSettings(FinalPerNodeColorGrading.PostProcessSettings, PerNodeColorGrading.ColorGradingSettings);
		}

		ImplUpdateCustomPostprocess(DstViewport, FinalPerNodeColorGrading.bIsEnabled, FinalPerNodeColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		return true;
	}

	return false;
}

static bool ImplUpdateIncameraAllNodesColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationViewport_AllNodesColorGrading AllNodesColorGrading)
{
	FDisplayClusterConfigurationViewport_CustomPostprocessSettings FinalAllNodesColorGrading;
	FinalAllNodesColorGrading.bIsEnabled = true;
	FinalAllNodesColorGrading.bIsOneFrame = true;
	FinalAllNodesColorGrading.BlendWeight = 1;

	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	// enable entire cluster only when global settings is on
	const bool bEnableEntireClusterColorGrading = StageSettings.EntireClusterColorGrading.bEnableEntireClusterColorGrading & AllNodesColorGrading.bEnableEntireClusterColorGrading;

	if (bEnableEntireClusterColorGrading)
	{
		FinalAllNodesColorGrading.BlendWeight = StageSettings.EntireClusterColorGrading.ColorGradingSettings.BlendWeight;
		
		FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(FinalAllNodesColorGrading.PostProcessSettings, StageSettings.EntireClusterColorGrading.ColorGradingSettings, AllNodesColorGrading.ColorGradingSettings);
		FinalAllNodesColorGrading.BlendWeight *= AllNodesColorGrading.ColorGradingSettings.BlendWeight;
	}
	else
	{
		FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyBlendPostProcessSettings(FinalAllNodesColorGrading.PostProcessSettings, AllNodesColorGrading.ColorGradingSettings);
	}

	ImplUpdateCustomPostprocess(DstViewport, FinalAllNodesColorGrading.bIsEnabled, FinalAllNodesColorGrading, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);

	return true;
}

#if WITH_EDITOR
// return true when same settings used for both viewports
bool FDisplayClusterViewportConfigurationHelpers_Postprocess::IsInnerFrustumViewportSettingsEqual_Editor(const FDisplayClusterViewport& InViewport1, const FDisplayClusterViewport& InViewport2, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	for (const FDisplayClusterConfigurationViewport_PerNodeColorGrading& ColorGradingProfileIt : CameraSettings.PerNodeColorGrading)
	{
		if (ColorGradingProfileIt.bIsEnabled)
		{
			const FString* CustomNode1 = ColorGradingProfileIt.ApplyPostProcessToObjects.FindByPredicate([ClusterNodeId = InViewport1.GetClusterNodeId()](const FString& InClusterNodeId)
			{
				return ClusterNodeId.Equals(InClusterNodeId, ESearchCase::IgnoreCase);
			});

			const FString* CustomNode2 = ColorGradingProfileIt.ApplyPostProcessToObjects.FindByPredicate([ClusterNodeId = InViewport2.GetClusterNodeId()](const FString& InClusterNodeId)
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

	return true;
}
#endif

bool FDisplayClusterViewportConfigurationHelpers_Postprocess::ImplUpdateInnerFrustumColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	const FDisplayClusterRenderFrameSettings& RenderFrameSettings = DstViewport.GetRenderFrameSettings();

	// per node color grading first (it includes all nodes blending too)
	const FString& ClusterNodeId = DstViewport.GetClusterNodeId();
	check(!ClusterNodeId.IsEmpty());

	for (const FDisplayClusterConfigurationViewport_PerNodeColorGrading& ColorGradingProfileIt : CameraSettings.PerNodeColorGrading)
	{
		// Only allowed profiles
		if (ColorGradingProfileIt.bIsEnabled)
		{
			for (const FString& ClusterNodeIt : ColorGradingProfileIt.ApplyPostProcessToObjects)
			{
				if (ClusterNodeId.Compare(ClusterNodeIt, ESearchCase::IgnoreCase) == 0)
				{
					// Use cluster node PP
					return ImplUpdateIncameraPerNodeColorGrading(DstViewport, RootActor, CameraSettings.AllNodesColorGrading, ColorGradingProfileIt);
				}
			}
		}
	}

	// run through dedicated all nodes pass only when per node list is empty
	if (CameraSettings.AllNodesColorGrading.bEnableInnerFrustumAllNodesColorGrading)
	{
		// all nodes only color grading
		return ImplUpdateIncameraAllNodesColorGrading(DstViewport, RootActor, CameraSettings.AllNodesColorGrading);
	}

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateLightcardPostProcessSettings(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_LightcardSettings& LightcardSettings = StageSettings.Lightcard;

	// First try use global OCIO from stage settings
	if (LightcardSettings.bEnableOuterViewportColorGrading)
	{
		if (ImplUpdateViewportColorGrading(DstViewport, RootActor, BaseViewport.GetId()))
		{
			return true;
		}
	}

	// This viewport doesn't use PP
	ImplRemoveCustomPostprocess(DstViewport, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);

	return false;
}

bool FDisplayClusterViewportConfigurationHelpers_Postprocess::ImplUpdateViewportColorGrading(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FString& InClusterViewportId)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	if (StageSettings.EnableColorGrading == false)
	{
		return false;
	}
	
	for (const FDisplayClusterConfigurationViewport_PerViewportColorGrading& ColorGradingProfileIt : StageSettings.PerViewportColorGrading)
	{
		if (ColorGradingProfileIt.bIsEnabled)
		{
			for (const FString& ViewportNameIt : ColorGradingProfileIt.ApplyPostProcessToObjects)
			{
				if (InClusterViewportId.Compare(ViewportNameIt, ESearchCase::IgnoreCase) == 0)
				{
					// Use per viewport blending
					return ImplUpdatePerViewportColorGrading(DstViewport, RootActor, ColorGradingProfileIt);
				}
			}
		}
	}

	// per viewport color grading is empty, entire cluster only
	return ImplUpdateEntireClusterColorGrading(DstViewport, RootActor);
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateCameraPostProcessSettings(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	FDisplayClusterConfigurationViewport_CustomPostprocessSettings CameraPPS;
	CameraPPS.bIsOneFrame = true;
	CameraPPS.BlendWeight = 1.f;

	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	if (CameraSettings.RenderSettings.bUseCameraComponentPostprocess)
	{
		FMinimalViewInfo DesiredView;
		InCameraComponent.GetDesiredView(DesiredView);

		// Send camera postprocess to override
		CameraPPS.bIsEnabled = true;
		CameraPPS.PostProcessSettings = DesiredView.PostProcessSettings;
	}

	const FDisplayClusterConfigurationICVFX_CameraMotionBlurOverridePPS& OverrideMotionBlurPPS = CameraSettings.CameraMotionBlur.MotionBlurPPS;
	if (OverrideMotionBlurPPS.bReplaceEnable)
	{
		// Send camera postprocess to override
		CameraPPS.bIsEnabled = true;

		CameraPPS.PostProcessSettings.MotionBlurAmount = OverrideMotionBlurPPS.MotionBlurAmount;
		CameraPPS.PostProcessSettings.bOverride_MotionBlurAmount = true;

		CameraPPS.PostProcessSettings.MotionBlurMax = OverrideMotionBlurPPS.MotionBlurMax;
		CameraPPS.PostProcessSettings.bOverride_MotionBlurMax = true;
		
		CameraPPS.PostProcessSettings.MotionBlurPerObjectSize = OverrideMotionBlurPPS.MotionBlurPerObjectSize;
		CameraPPS.PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
	}

	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	// check if frustum color grading is enabled	
	if (StageSettings.EnableColorGrading && CameraSettings.EnableInnerFrustumColorGrading)
	{
		ImplUpdateCustomPostprocess(DstViewport, CameraPPS.bIsEnabled, CameraPPS, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override);

		if (!ImplUpdateInnerFrustumColorGrading(DstViewport, RootActor, InCameraComponent))
		{
			// This viewport doesn't use per-viewport PP
			ImplRemoveCustomPostprocess(DstViewport, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateCustomPostProcessSettings(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationViewport_CustomPostprocess& InCustomPostprocessConfiguration)
{
	// update postprocess settings (Start, Override, Final)
	ImplUpdateCustomPostprocess(DstViewport, InCustomPostprocessConfiguration.Start.bIsEnabled, InCustomPostprocessConfiguration.Start, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Start);
	ImplUpdateCustomPostprocess(DstViewport, InCustomPostprocessConfiguration.Override.bIsEnabled, InCustomPostprocessConfiguration.Override, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override);
	ImplUpdateCustomPostprocess(DstViewport, InCustomPostprocessConfiguration.Final.bIsEnabled, InCustomPostprocessConfiguration.Final, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Final);
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdatePerViewportPostProcessSettings(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor)
{
	if (!ImplUpdateViewportColorGrading(DstViewport, RootActor, DstViewport.GetId()))
	{
		// This viewport doesn't use PP
		ImplRemoveCustomPostprocess(DstViewport, IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport);
	}
}

// Note that skipped parameters in macro definitions will just evaluate to nothing
// This is intentional to get around the inconsistent naming in the color grading fields in FPostProcessSettings
#define PP_CONDITIONAL_BLEND(BLENDOP, COLOR, OUTGROUP, INGROUP, NAME, OFFSETOP, OFFSETVALUE) \
	{ \
		bool bOverridePPSettings0 = PPSettings0.INGROUP bOverride_##NAME; \
		bool bOverridePPSettings1 = (PPSettings1 != nullptr) && PPSettings1->INGROUP bOverride_##NAME; \
		bool bOverridePPSettings2 = (PPSettings2 != nullptr) && PPSettings2->INGROUP bOverride_##NAME; \
		 \
		if (bOverridePPSettings0 && bOverridePPSettings1 && bOverridePPSettings2) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings0.INGROUP NAME BLENDOP PPSettings1->INGROUP NAME BLENDOP PPSettings2->INGROUP NAME OFFSETOP OFFSETVALUE OFFSETOP OFFSETVALUE; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings0 && bOverridePPSettings1) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings0.INGROUP NAME BLENDOP PPSettings1->INGROUP NAME OFFSETOP OFFSETVALUE; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings0 && bOverridePPSettings2) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings0.INGROUP NAME BLENDOP PPSettings2->INGROUP NAME OFFSETOP OFFSETVALUE; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings1 && bOverridePPSettings2) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings1->INGROUP NAME BLENDOP PPSettings2->INGROUP NAME OFFSETOP OFFSETVALUE; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings2) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings2->INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings1) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings1->INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		else if (bOverridePPSettings0) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings0.INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
	} \

/* 
* This will override the settings using the priority. 
* bOverridePPSettings2 (any additional settings specified by the user) will be of highest priority.
* bOverridePPSettings1 (which is nDisplay override settings) will be of highest priority.
* following by Cumulative settings (bOverridePPSettings0).
*/
#define PP_CONDITIONAL_OVERRIDE(COLOR, OUTGROUP, INGROUP, NAME) \
	{ \
		bool bOverridePPSettings0 = PPSettings0.INGROUP bOverride_##NAME; \
		bool bOverridePPSettings1 = PPSettings1 && PPSettings1->INGROUP bOverride_##NAME; \
		bool bOverridePPSettings2 = PPSettings2 && PPSettings2->INGROUP bOverride_##NAME; \
		if (bOverridePPSettings0) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings0.INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		if (bOverridePPSettings1) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings1->INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
		if (bOverridePPSettings2) \
		{ \
			OutputPP.COLOR##NAME##OUTGROUP = PPSettings2->INGROUP NAME; \
			OutputPP.bOverride_##COLOR##NAME##OUTGROUP = true; \
		} \
	} \

static void ImplBlendPostProcessSettings(FPostProcessSettings& OutputPP, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& PPSettings0, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings* PPSettings1, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings* PPSettings2)
{
	PP_CONDITIONAL_BLEND(+, , , , AutoExposureBias, , );
	PP_CONDITIONAL_BLEND(+, , , , ColorCorrectionHighlightsMin, , );
	PP_CONDITIONAL_BLEND(+, , , , ColorCorrectionHighlightsMax, , );
	PP_CONDITIONAL_BLEND(+, , , , ColorCorrectionShadowsMax, , );

	PP_CONDITIONAL_OVERRIDE(, , WhiteBalance., TemperatureType);
	PP_CONDITIONAL_BLEND(+, , , WhiteBalance., WhiteTemp, +, -6500.0f);
	PP_CONDITIONAL_BLEND(+, , , WhiteBalance., WhiteTint, , );

	PP_CONDITIONAL_BLEND(*, Color, , Global., Saturation, , );
	PP_CONDITIONAL_BLEND(*, Color, , Global., Contrast, , );
	PP_CONDITIONAL_BLEND(*, Color, , Global., Gamma, , );
	PP_CONDITIONAL_BLEND(*, Color, , Global., Gain, , );
	PP_CONDITIONAL_BLEND(+, Color, , Global., Offset, , );

	PP_CONDITIONAL_BLEND(*, Color, Shadows, Shadows., Saturation, , );
	PP_CONDITIONAL_BLEND(*, Color, Shadows, Shadows., Contrast, , );
	PP_CONDITIONAL_BLEND(*, Color, Shadows, Shadows., Gamma, , );
	PP_CONDITIONAL_BLEND(*, Color, Shadows, Shadows., Gain, , );
	PP_CONDITIONAL_BLEND(+, Color, Shadows, Shadows., Offset, , );

	PP_CONDITIONAL_BLEND(*, Color, Midtones, Midtones., Saturation, , );
	PP_CONDITIONAL_BLEND(*, Color, Midtones, Midtones., Contrast, , );
	PP_CONDITIONAL_BLEND(*, Color, Midtones, Midtones., Gamma, , );
	PP_CONDITIONAL_BLEND(*, Color, Midtones, Midtones., Gain, , );
	PP_CONDITIONAL_BLEND(+, Color, Midtones, Midtones., Offset, , );

	PP_CONDITIONAL_BLEND(*, Color, Highlights, Highlights., Saturation, , );
	PP_CONDITIONAL_BLEND(*, Color, Highlights, Highlights., Contrast, , );
	PP_CONDITIONAL_BLEND(*, Color, Highlights, Highlights., Gamma, , );
	PP_CONDITIONAL_BLEND(*, Color, Highlights, Highlights., Gain, , );
	PP_CONDITIONAL_BLEND(+, Color, Highlights, Highlights., Offset, , );

	PP_CONDITIONAL_BLEND(+, , , Misc., BlueCorrection, , );
	PP_CONDITIONAL_BLEND(+, , , Misc., ExpandGamut, , );
	PP_CONDITIONAL_BLEND(+, , , Misc., SceneColorTint, , );
}
void FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyBlendPostProcessSettings(FPostProcessSettings& OutputPP, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& InPPSettings)
{
	ImplBlendPostProcessSettings(OutputPP, InPPSettings, nullptr, nullptr);
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::PerNodeBlendPostProcessSettings(FPostProcessSettings& OutputPP, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& ClusterPPSettings, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& ViewportPPSettings, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& PerNodePPSettings)
{
	ImplBlendPostProcessSettings(OutputPP, ClusterPPSettings, &ViewportPPSettings, &PerNodePPSettings);
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(FPostProcessSettings& OutputPP, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& ClusterPPSettings, const FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings& ViewportPPSettings)
{
	ImplBlendPostProcessSettings(OutputPP, ClusterPPSettings, &ViewportPPSettings, nullptr);
}

#define PP_CONDITIONAL_COPY(COLOR, OUTGROUP, INGROUP, NAME) \
		if (!bIsConditionalCopy || InPPS->bOverride_##COLOR##NAME##INGROUP) \
		{ \
			OutViewportPPSettings->OUTGROUP NAME = InPPS->COLOR##NAME##INGROUP; \
			OutViewportPPSettings->OUTGROUP bOverride_##NAME = true; \
		}

static void ImplCopyPPSStruct(bool bIsConditionalCopy, FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings* OutViewportPPSettings, FPostProcessSettings* InPPS)
{
	if ((OutViewportPPSettings != nullptr) && (InPPS != nullptr))
	{
		PP_CONDITIONAL_COPY(, , , AutoExposureBias);
		PP_CONDITIONAL_COPY(, , , ColorCorrectionHighlightsMin);
		PP_CONDITIONAL_COPY(, , , ColorCorrectionHighlightsMax);
		PP_CONDITIONAL_COPY(, , , ColorCorrectionShadowsMax);

		PP_CONDITIONAL_COPY(, WhiteBalance., , TemperatureType);
		PP_CONDITIONAL_COPY(, WhiteBalance., , WhiteTemp);
		PP_CONDITIONAL_COPY(, WhiteBalance., , WhiteTint);

		PP_CONDITIONAL_COPY(Color, Global., , Saturation);
		PP_CONDITIONAL_COPY(Color, Global., , Contrast);
		PP_CONDITIONAL_COPY(Color, Global., , Gamma);
		PP_CONDITIONAL_COPY(Color, Global., , Gain);
		PP_CONDITIONAL_COPY(Color, Global., , Offset);

		PP_CONDITIONAL_COPY(Color, Shadows., Shadows, Saturation);
		PP_CONDITIONAL_COPY(Color, Shadows., Shadows, Contrast);
		PP_CONDITIONAL_COPY(Color, Shadows., Shadows, Gamma);
		PP_CONDITIONAL_COPY(Color, Shadows., Shadows, Gain);
		PP_CONDITIONAL_COPY(Color, Shadows., Shadows, Offset);

		PP_CONDITIONAL_COPY(Color, Midtones., Midtones, Saturation);
		PP_CONDITIONAL_COPY(Color, Midtones., Midtones, Contrast);
		PP_CONDITIONAL_COPY(Color, Midtones., Midtones, Gamma);
		PP_CONDITIONAL_COPY(Color, Midtones., Midtones, Gain);
		PP_CONDITIONAL_COPY(Color, Midtones., Midtones, Offset);

		PP_CONDITIONAL_COPY(Color, Highlights., Highlights, Saturation);
		PP_CONDITIONAL_COPY(Color, Highlights., Highlights, Contrast);
		PP_CONDITIONAL_COPY(Color, Highlights., Highlights, Gamma);
		PP_CONDITIONAL_COPY(Color, Highlights., Highlights, Gain);
		PP_CONDITIONAL_COPY(Color, Highlights., Highlights, Offset);

		PP_CONDITIONAL_COPY(, Misc., , BlueCorrection);
		PP_CONDITIONAL_COPY(, Misc., , ExpandGamut);
		PP_CONDITIONAL_COPY(, Misc., , SceneColorTint);
	}
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStructConditional(FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings* OutViewportPPSettings, FPostProcessSettings* InPPS)
{
	ImplCopyPPSStruct(true, OutViewportPPSettings, InPPS);
}

void FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStruct(FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings* OutViewportPPSettings, FPostProcessSettings* InPPS)
{
	ImplCopyPPSStruct(false, OutViewportPPSettings, InPPS);
	}
