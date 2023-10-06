// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterPreviewComponent.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Misc/DisplayClusterLog.h"

#if WITH_EDITOR

#include "EngineModule.h"
#include "CanvasTypes.h"
#include "LegacyScreenPercentageDriver.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

#include "Engine/Scene.h"
#include "GameFramework/WorldSettings.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"

///////////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterPreviewEnableViewState = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableViewState(
	TEXT("nDisplay.preview.EnableViewState"),
	GDisplayClusterPreviewEnableViewState,
	TEXT("Enable view state for preview (0 - disable).\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewEnableConfiguratorViewState = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableConfiguratorViewState(
	TEXT("nDisplay.preview.EnableConfiguratorViewState"),
	GDisplayClusterPreviewEnableConfiguratorViewState,
	TEXT("Enable view state for preview in Configurator window (0 - disable).\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewport::CleanupViewState()
{
	for (TSharedPtr<FSceneViewStateReference, ESPMode::ThreadSafe>& ViewState : ViewStates)
	{
		if (ViewState.IsValid())
		{
			FSceneViewStateInterface* Ref = ViewState->GetReference();
			if (Ref != nullptr)
			{
				Ref->ClearMIDPool();
			}
		}
	}
}

FSceneViewStateInterface* FDisplayClusterViewport::GetViewState(uint32 ViewIndex)
{
	if (GDisplayClusterPreviewEnableViewState == 0 || (GDisplayClusterPreviewEnableConfiguratorViewState == 0 && IsCurrentWorldHasAnyType(EWorldType::EditorPreview)))
	{
		// Disable ViewState
		ViewStates.Empty();

		return nullptr;
	}

	int32 RequiredAmount = (int32)ViewIndex - ViewStates.Num() + 1;
	if (RequiredAmount > 0)
	{
		ViewStates.AddDefaulted(RequiredAmount);
	}

	if (!ViewStates[ViewIndex].IsValid())
	{
		ViewStates[ViewIndex] = MakeShared<FSceneViewStateReference>();
	}

	if (ViewStates[ViewIndex]->GetReference() == NULL)
	{
		const UWorld* CurrentWorld = GetCurrentWorld();
		const ERHIFeatureLevel::Type FeatureLevel = CurrentWorld ? CurrentWorld->GetFeatureLevel() : GMaxRHIFeatureLevel;

		ViewStates[ViewIndex]->Allocate(FeatureLevel);
	}

	return ViewStates[ViewIndex]->GetReference();
}

FSceneView* FDisplayClusterViewport::ImplCalcScenePreview(FSceneViewFamilyContext& InOutViewFamily, uint32 InContextNum)
{
	check(InContextNum >= 0 && InContextNum < (uint32)Contexts.Num());

	const float WorldToMeters = 100.f;
	const float MaxViewDistance = 1000000;
	const bool bUseSceneColorTexture = false;
	const float LODDistanceFactor = 1.0f;

	const AActor* ViewOwner = nullptr;

	FVector  ViewLocation;
	FRotator ViewRotation;

	if (ImplPreview_CalculateStereoViewOffset(InContextNum, ViewRotation, WorldToMeters, ViewLocation))
	{
		FMatrix ProjectionMatrix = ImplPreview_GetStereoProjectionMatrix(InContextNum);

		FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewRotation);
		ViewRotationMatrix = ViewRotationMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		FIntRect ViewRect = Contexts[InContextNum].RenderTargetRect;

		FSceneViewInitOptions ViewInitOptions;

		ViewInitOptions.SetViewRectangle(ViewRect);
		ViewInitOptions.ViewFamily = &InOutViewFamily;

		ViewInitOptions.SceneViewStateInterface = GetViewState(InContextNum);
		ViewInitOptions.ViewActor = ViewOwner;

		ViewInitOptions.ViewOrigin = ViewLocation;
		ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;

		ViewInitOptions.OverrideFarClippingPlaneDistance = MaxViewDistance;
		ViewInitOptions.StereoPass = Contexts[InContextNum].StereoscopicPass;
		ViewInitOptions.StereoViewIndex = Contexts[InContextNum].StereoViewIndex;

		ViewInitOptions.LODDistanceFactor = FMath::Clamp(LODDistanceFactor, 0.01f, 100.0f);

		if (InOutViewFamily.Scene->GetWorld() != nullptr && InOutViewFamily.Scene->GetWorld()->GetWorldSettings() != nullptr)
		{
			ViewInitOptions.WorldToMetersScale = InOutViewFamily.Scene->GetWorld()->GetWorldSettings()->WorldToMeters;
		}

		ViewInitOptions.BackgroundColor = FLinearColor::Black;

		if (const FDisplayClusterRenderFrameSettings* RenderFrameSettings = GetRenderFrameSettings())
		{
			if (RenderFrameSettings->bPreviewEnablePostProcess == false)
			{
				ViewInitOptions.OverlayColor = FLinearColor::Black;
			}
		}

		ViewInitOptions.bSceneCaptureUsesRayTracing = false;
		ViewInitOptions.bIsPlanarReflection = false;

		FSceneView* View = new FSceneView(ViewInitOptions);

		InOutViewFamily.Views.Add(View);

		View->StartFinalPostprocessSettings(ViewLocation);

		FPostProcessSettings* FinalPostProcessingSettings = &View->FinalPostProcessSettings;
		// Support start PPS for preview
		GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Start, FinalPostProcessingSettings);

		// Support override PPS for preview
		FPostProcessSettings OverridePostProcessingSettings;
		float OverridePostProcessBlendWeight = 1.0f;
		GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override, &OverridePostProcessingSettings, &OverridePostProcessBlendWeight);

		View->OverridePostProcessSettings(OverridePostProcessingSettings, OverridePostProcessBlendWeight);

		// Support final PPS for preview
		GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Final, FinalPostProcessingSettings);

		FPostProcessSettings RequestedFinalPerViewportPPS;
		// Get the final overall cluster + per-viewport PPS from nDisplay
		if (GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport, &RequestedFinalPerViewportPPS))
		{
			FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings InPPSnDisplay;
			FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStructConditional(&InPPSnDisplay, &RequestedFinalPerViewportPPS);

			// Get the passed-in cumulative PPS from the game/viewport (includes all PPVs affecting this viewport)
			FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings InPPSCumulative;
			FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStruct(&InPPSCumulative, FinalPostProcessingSettings);

			// Blend both together with our custom math instead of the default PPS blending
			FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(*FinalPostProcessingSettings, InPPSCumulative, InPPSnDisplay);
		}

		View->EndFinalPostprocessSettings(ViewInitOptions);

		// Setup view extension for this view
		for (int32 ViewExt = 0; ViewExt < InOutViewFamily.ViewExtensions.Num(); ViewExt++)
		{
			InOutViewFamily.ViewExtensions[ViewExt]->SetupView(InOutViewFamily, *View);
		}

		return View;
	}

	return nullptr;
}

bool FDisplayClusterViewport::ImplPreview_CalculateStereoViewOffset(const uint32 InContextNum, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	check(IsInGameThread());
	check(WorldToMeters > 0.f);

	// Obtaining the internal viewpoint for a given viewport with stereo eye offset distance.
	FMinimalViewInfo ViewInfo;
	if (!SetupViewPoint(ViewInfo))
	{
		return false;
	}

	ViewLocation = ViewInfo.Location;
	ViewRotation = ViewInfo.Rotation;

	const float PassOffsetSwap = GetStereoEyeOffsetDistance(InContextNum);

	FVector ViewOffset = FVector::ZeroVector;

	// Apply computed offset to the view location
	const FQuat EyeQuat = ViewRotation.Quaternion();
	ViewOffset = EyeQuat.RotateVector(FVector(0.0f, PassOffsetSwap, 0.0f));
	ViewLocation += ViewOffset;

	// Perform view calculations on a policy side
	const float CfgNCP = GNearClippingPlane;
	if (!CalculateView(InContextNum, ViewLocation, ViewRotation, ViewOffset, WorldToMeters, CfgNCP, CfgNCP))
	{
		if (!bProjectionPolicyCalculateViewWarningOnce)
		{
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Couldn't compute view parameters for Viewport %s, ViewIdx: %d"), *GetId(), InContextNum);
			bProjectionPolicyCalculateViewWarningOnce = true;
		}
		return false;
	}

	bProjectionPolicyCalculateViewWarningOnce = false;

	UE_LOG(LogDisplayClusterViewport, VeryVerbose, TEXT("ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());

	return true;
}

FMatrix FDisplayClusterViewport::ImplPreview_GetStereoProjectionMatrix(const uint32 InContextNum)
{
	check(IsInGameThread());

	FMatrix PrjMatrix = FMatrix::Identity;

	if (IsSceneOpened())
	{
		if (GetProjectionMatrix(InContextNum, PrjMatrix) == false)
		{
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Got invalid projection matrix: Viewport %s, ViewIdx: %d"), *GetId(), InContextNum);
		}
	}

	return PrjMatrix;
}

bool FDisplayClusterViewport::GetPreviewPixels(TSharedPtr<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe>& OutPixelsData) const
{
	check(IsInGameThread());

	return (ViewportProxy != nullptr) && ViewportProxy->GetPreviewPixels_GameThread(OutPixelsData);
}
#endif
