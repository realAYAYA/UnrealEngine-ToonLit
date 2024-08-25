// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/AvaGameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraPhotography.h"
#include "Components/LineBatchComponent.h"
#include "ContentStreaming.h"
#include "DynamicResolutionState.h"
#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "IAvaModule.h"
#include "IHeadMountedDisplay.h"
#include "IXRCamera.h"
#include "IXRTrackingSystem.h"
#include "LegacyScreenPercentageDriver.h"
#include "Misc/EngineVersionComparison.h"
#include "SceneViewExtension.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(), *CanvasName.ToString(), false);
		if (!CanvasObject)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

UAvaGameViewportClient::UAvaGameViewportClient()
	: CameraManager(MakeShared<FAvaCameraManager>())
{
}

void UAvaGameViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	// Override the Canvas Render Target with ours
	if (RenderTarget.IsValid())
	{
		Canvas->SetRenderTarget_GameThread(RenderTarget->GameThread_GetRenderTargetResource());
	}
	
	// Allow HMD to modify the view later, just before rendering
	const bool bStereoRendering = GEngine->IsStereoscopic3D(InViewport);
	
	// Create a temporary canvas if there isn't already one.
	static FName CanvasObjectName(TEXT("CanvasObject"));
	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
	CanvasObject->Canvas = Canvas;

	// Create temp debug canvas object
	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();
	FIntPoint DebugCanvasSize = InViewport->GetSizeXY();
	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		DebugCanvasSize = GEngine->XRSystem->GetHMDDevice()->GetIdealDebugCanvasRenderTargetSize();
	}

	static FName DebugCanvasObjectName(TEXT("DebugCanvasObject"));
	UCanvas* DebugCanvasObject = GetCanvasByName(DebugCanvasObjectName);
	DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, nullptr, DebugCanvas);
	if (DebugCanvas)
	{
		DebugCanvas->SetScaledToRenderTarget(bStereoRendering);
		DebugCanvas->SetStereoRendering(bStereoRendering);
	}
	Canvas->SetScaledToRenderTarget(bStereoRendering);
	Canvas->SetStereoRendering(bStereoRendering);
	
	if (World == nullptr)
	{
		return;
	}

	constexpr bool bCaptureNeedsSceneColor = false;
	constexpr ESceneCaptureSource SceneCaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	constexpr ESceneCaptureCompositeMode SceneCaptureCompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;

	/** When enabled, the scene capture will composite into the render target instead of overwriting its contents. */

	//TODO: World->GetTime() seems to return zeros because we have not Begun Play...
	FGameTime Time = FGameTime::GetTimeSinceAppStart();

	// Setup a FSceneViewFamily/FSceneView for the viewport.
	FSceneViewFamilyContext ViewFamilyContext(FSceneViewFamily::ConstructionValues(Canvas->GetRenderTarget()
		, World->Scene
		, EngineShowFlags)
		.SetRealtimeUpdate(true)
		.SetResolveScene(!bCaptureNeedsSceneColor)
		.SetTime(Time));

	ViewFamilyContext.SceneCaptureSource = SceneCaptureSource;
	ViewFamilyContext.SceneCaptureCompositeMode = SceneCaptureCompositeMode;
	ViewFamilyContext.DebugDPIScale = GetDPIScale();
	ViewFamilyContext.EngineShowFlags = EngineShowFlags;

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Force enable view family show flag for HighDPI derived's screen percentage.
		ViewFamilyContext.EngineShowFlags.ScreenPercentage = true;
	}
#endif

	if (!bStereoRendering)
	{
		// stereo is enabled, as many HMDs require this for proper visuals
		ViewFamilyContext.EngineShowFlags.SetScreenPercentage(false);
	}

	ViewFamilyContext.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(InViewport));
	
	for (const TSharedRef<ISceneViewExtension>& ViewExt : ViewFamilyContext.ViewExtensions)
	{
		ViewExt->SetupViewFamily(ViewFamilyContext);
	}

	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		// Allow HMD to modify screen settings
		GEngine->XRSystem->GetHMDDevice()->UpdateScreenSettings(InViewport);
	}

	ViewFamilyContext.ViewMode = EViewModeIndex::VMI_Lit;
	EngineShowFlagOverride(ESFIM_Game, ViewFamilyContext.ViewMode, ViewFamilyContext.EngineShowFlags, false);

	bool bFinalScreenPercentageShowFlag;
	bool bUsesDynamicResolution = false;
	// Setup the screen percentage and upscaling method for the view family.
	{
		checkf(ViewFamilyContext.GetScreenPercentageInterface() == nullptr,
			TEXT("Some code has tried to set up an alien screen percentage driver, that could be wrong if not supported very well by the RHI."));

		// Force screen percentage show flag to be turned off if not supported.
		if (!ViewFamilyContext.SupportsScreenPercentage())
		{
			ViewFamilyContext.EngineShowFlags.ScreenPercentage = false;
		}

		// Set up secondary resolution fraction for the view family.
		if (!bStereoRendering && ViewFamilyContext.SupportsScreenPercentage())
		{
			// Automatically compute secondary resolution fraction from DPI.
			ViewFamilyContext.SecondaryViewFraction = GetDPIDerivedResolutionFraction();
		}

		bFinalScreenPercentageShowFlag = ViewFamilyContext.EngineShowFlags.ScreenPercentage;
	}

	TArray<FSceneView*> Views;
	{
		const int32 NumViews = bStereoRendering ? GEngine->StereoRenderingDevice->GetDesiredNumberOfViews(bStereoRendering) : 1;
		for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
		{
			FSceneView* const View = CalcSceneView(&ViewFamilyContext
				, InViewport
				, nullptr
				, bStereoRendering ? ViewIndex : INDEX_NONE);

			if (View)
			{
				Views.Add(View);

				View->CameraConstrainedViewRect = View->UnscaledViewRect;

				// Add view information for resource streaming. Allow up to 5X boost for small FOV.
				const float StreamingScale = 1.f / FMath::Clamp<float>(View->LODDistanceFactor, .2f, 1.f);

				IStreamingManager::Get().AddViewInformation(View->ViewMatrices.GetViewOrigin()
					, View->UnscaledViewRect.Width()
					, View->UnscaledViewRect.Width() * View->ViewMatrices.GetProjectionMatrix().M[0][0]
					, StreamingScale);

				World->ViewLocationsRenderedLastFrame.Add(View->ViewMatrices.GetViewOrigin());
			}
		}
	}

	//FinalizeViews(&ViewFamily); TODO Possible Interface

	// Update level streaming.
	World->UpdateLevelStreaming();

	Canvas->Clear(FLinearColor::Transparent);

	// If a screen percentage interface was not set by one of the view extension, then set the legacy one.
	if (ViewFamilyContext.GetScreenPercentageInterface() == nullptr)
	{
		float GlobalResolutionFraction = 1.0f;

		if (ViewFamilyContext.EngineShowFlags.ScreenPercentage && !bDisableWorldRendering && ViewFamilyContext.Views.Num() > 0)
		{
			// Get global view fraction.
			FStaticResolutionFractionHeuristic StaticHeuristic;
			StaticHeuristic.Settings.PullRunTimeRenderingSettings(GetViewStatusForScreenPercentage());
			StaticHeuristic.PullViewFamilyRenderingSettings(ViewFamilyContext);
			StaticHeuristic.DPIScale = GetDPIScale();

			GlobalResolutionFraction = StaticHeuristic.ResolveResolutionFraction();
		}

		ViewFamilyContext.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamilyContext, GlobalResolutionFraction));
	}

	check(ViewFamilyContext.GetScreenPercentageInterface() != nullptr);

	// Make sure the engine show flag for screen percentage is still what it was when setting up the screen percentage interface
	ViewFamilyContext.EngineShowFlags.ScreenPercentage = bFinalScreenPercentageShowFlag;

	if (bStereoRendering && bUsesDynamicResolution)
	{
		// Change screen percentage method to raw output when doing dynamic resolution with VR if not using TAA upsample.
		for (FSceneView* const View : Views)
		{
			if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::SpatialUpscale)
			{
				View->PrimaryScreenPercentageMethod = EPrimaryScreenPercentageMethod::RawOutput;
			}
		}
	}

	ViewFamilyContext.bIsHDR = InViewport->IsHDRViewport();

	if (!bDisableWorldRendering && FSlateApplication::Get().GetPlatformApplication()->IsAllowedToRender()) //-V560
	{
		for (const TSharedRef<ISceneViewExtension>& ViewExt : ViewFamilyContext.ViewExtensions)
		{
			for (FSceneView* const View : Views)
			{
				ViewExt->SetupView(ViewFamilyContext, *View);
			}
		}
		GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamilyContext);
	}
	else
	{
		GetRendererModule().PerFrameCleanupIfSkipRenderer();
	}

	// Beyond this point, only UI rendering independent from dynamic resolution.
	GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::EndDynamicResolutionRendering);

	// Remove temporary debug lines.
	if (World->LineBatcher)
	{
		World->LineBatcher->Flush();
	}
	if (World->ForegroundLineBatcher)
	{
		World->ForegroundLineBatcher->Flush();
	}

	// Render Stats HUD in the main canvas so that it gets captured
	// and is displayed in the broadcast channel's outputs.
	if (!Views.IsEmpty() && IAvaModule::Get().ShouldShowRuntimeStats())
	{
		DrawStatsHUD( World, InViewport, Canvas, nullptr, DebugProperties, Views[0]->ViewLocation, Views[0]->ViewRotation);
	}
	
	// Ensure canvas has been flushed before rendering UI
	Canvas->Flush_GameThread();

	CameraManager->ResetCameraCut();
}

bool UAvaGameViewportClient::IsStatEnabled(const FString& InName) const
{
	// The IAvaModule holds the runtime stats. We want them persistent across all viewports.
	return IAvaModule::Get().IsRuntimeStatEnabled(InName);
}

void UAvaGameViewportClient::SetCameraCutThisFrame()
{
	bCameraCutThisFrame = true;
}

void UAvaGameViewportClient::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	RenderTarget = InRenderTarget;
}

bool UAvaGameViewportClient::CalcSceneViewInitOptions(FSceneViewInitOptions& OutInitOptions
	, FViewport* InViewport
	, FViewElementDrawer* ViewDrawer
	, int32 StereoViewIndex)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AvaMediaCalcSceneViewInitOptions);
	if (InViewport == nullptr || !World)
	{
		return false;
	}

	// get the projection data
	if (GetProjectionData(InViewport, /*inout*/ OutInitOptions, StereoViewIndex) == false)
	{
		// Return false if this we didn't get back the info we needed
		return false;
	}

	// return if we have an invalid view rect
	if (!OutInitOptions.IsValidViewRectangle())
	{
		return false;
	}

	// Was there a camera cut this frame?
	OutInitOptions.bInCameraCut = CameraManager->ShouldEnableCameraCut() || bCameraCutThisFrame;
	bCameraCutThisFrame = false;

	if (GEngine->StereoRenderingDevice.IsValid())
	{
		OutInitOptions.StereoPass = GEngine->StereoRenderingDevice->GetViewPassForIndex(StereoViewIndex != INDEX_NONE, StereoViewIndex);
	}

	const uint32 ViewIndex = StereoViewIndex != INDEX_NONE ? StereoViewIndex : 0;

	// Make sure the ViewStates array has enough elements for the given ViewIndex.
	{
		const int32 RequiredViewStates = (ViewIndex + 1) - ViewStates.Num();

		if (RequiredViewStates > 0)
		{
			ViewStates.AddDefaulted(RequiredViewStates);
		}
	}

	// Allocate the current ViewState if necessary
	if (ViewStates[ViewIndex].GetReference() == nullptr)
	{
#if UE_VERSION_OLDER_THAN(5, 3, 0)
		const ERHIFeatureLevel::Type FeatureLevel = World ? World->FeatureLevel.GetValue() : GMaxRHIFeatureLevel;
#else
		const ERHIFeatureLevel::Type FeatureLevel = World ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;
#endif
		ViewStates[ViewIndex].Allocate(FeatureLevel);
	}

	OutInitOptions.SceneViewStateInterface = ViewStates[ViewIndex].GetReference();
	OutInitOptions.ViewActor = CameraManager->GetViewTarget();
	OutInitOptions.PlayerIndex = 0;
	OutInitOptions.ViewElementDrawer = ViewDrawer;
	OutInitOptions.BackgroundColor = FLinearColor::Transparent;
	OutInitOptions.LODDistanceFactor = 1.f;
	OutInitOptions.StereoViewIndex = StereoViewIndex;
	OutInitOptions.WorldToMetersScale = World->GetWorldSettings()->WorldToMeters;
	OutInitOptions.CursorPos = InViewport->HasMouseCapture() ? FIntPoint(-1, -1) : FIntPoint(InViewport->GetMouseX(), InViewport->GetMouseY());
	OutInitOptions.OriginOffsetThisFrame = World->OriginOffsetThisFrame;
	return true;
}

bool UAvaGameViewportClient::GetProjectionData(FViewport* InViewport
	, FSceneViewProjectionData& ProjectionData
	, int32 StereoViewIndex) const
{
		// If the actor
	if ((InViewport == nullptr) || (InViewport->GetSizeXY().X == 0) || (InViewport->GetSizeXY().Y == 0))
	{
		return false;
	}

	const FVector2D Origin(0.f, 0.f);
	const FVector2D Size(1.f, 1.f);

	int32 X = FMath::TruncToInt(Origin.X * InViewport->GetSizeXY().X);
	int32 Y = FMath::TruncToInt(Origin.Y * InViewport->GetSizeXY().Y);

	X += InViewport->GetInitialPositionXY().X;
	Y += InViewport->GetInitialPositionXY().Y;

	uint32 SizeX = FMath::TruncToInt(Size.X * InViewport->GetSizeXY().X);
	uint32 SizeY = FMath::TruncToInt(Size.Y * InViewport->GetSizeXY().Y);

	FIntRect UnconstrainedRectangle = FIntRect(X, Y, X + SizeX, Y + SizeY);

	ProjectionData.SetViewRectangle(UnconstrainedRectangle);

	// Get the viewpoint.
	FMinimalViewInfo ViewInfo;
	CameraManager->SetCameraViewInfo(ViewInfo);

	// If stereo rendering is enabled, update the size and offset appropriately for this pass
	const bool bNeedStereo = StereoViewIndex != INDEX_NONE && GEngine->IsStereoscopic3D();
	const bool bIsHeadTrackingAllowed = GEngine->XRSystem.IsValid()
		&& (World != nullptr
			? GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*World)
			: GEngine->XRSystem->IsHeadTrackingAllowed());

	if (bNeedStereo)
	{
		GEngine->StereoRenderingDevice->AdjustViewRect(StereoViewIndex, X, Y, SizeX, SizeY);
	}

    FVector StereoViewLocation = ViewInfo.Location;
    if (bNeedStereo || bIsHeadTrackingAllowed)
    {
	    TSharedPtr<IXRCamera> XRCamera = GEngine->XRSystem.IsValid()
    		? GEngine->XRSystem->GetXRCamera()
    		: nullptr;

		if (XRCamera.IsValid())
		{
			const bool bHasActiveCamera = !!CameraManager->GetCachedCameraComponent();
			XRCamera->UseImplicitHMDPosition(bHasActiveCamera);
		}

		if (GEngine->StereoRenderingDevice.IsValid())
		{
			GEngine->StereoRenderingDevice->CalculateStereoViewOffset(StereoViewIndex, ViewInfo.Rotation, World->GetWorldSettings()->WorldToMeters, StereoViewLocation);
		}
    }

	// Create the view matrix
	ProjectionData.ViewOrigin = StereoViewLocation;
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
		FPlane(0,	0,	1,	0),
		FPlane(1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	// @todo viewext this use case needs to be revisited
	if (!bNeedStereo)
	{
		// Create the projection matrix (and possibly constrain the view rectangle)
		FMinimalViewInfo::CalculateProjectionMatrixGivenView(ViewInfo
			, GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint
			, InViewport
			, ProjectionData);

		for (const TSharedRef<ISceneViewExtension>& ViewExt : GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(InViewport)))
        {
			ViewExt->SetupViewProjectionMatrix(ProjectionData);
		};
	}
	else
	{
		// Let the stereoscopic rendering device handle creating its own projection matrix, as needed
		ProjectionData.ProjectionMatrix = GEngine->StereoRenderingDevice->GetStereoProjectionMatrix(StereoViewIndex);

		// calculate the out rect
		ProjectionData.SetViewRectangle(FIntRect(X, Y, X + SizeX, Y + SizeY));
	}
	return true;
}

FSceneView* UAvaGameViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily
	, FViewport* InViewport
	, FViewElementDrawer* ViewDrawer
	, int32 StereoViewIndex)
{
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_AvaMediaCalcSceneView);
	FSceneViewInitOptions ViewInitOptions;
	if (!CalcSceneViewInitOptions(ViewInitOptions, InViewport, ViewDrawer, StereoViewIndex))
	{
		return nullptr;
	}

	// Get the viewpoint...technically doing this twice
	// but it makes GetProjectionData better
	FMinimalViewInfo ViewInfo;
	CameraManager->SetCameraViewInfo(ViewInfo);

	ViewInitOptions.ViewLocation = ViewInfo.Location;
	ViewInitOptions.ViewRotation = ViewInfo.Rotation;
	ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;
	ViewInitOptions.FOV = ViewInfo.FOV;
	ViewInitOptions.DesiredFOV = ViewInfo.DesiredFOV;

	// Fill out the rest of the view init options
	ViewInitOptions.ViewFamily = ViewFamily;

	FSceneView* const View = new FSceneView(ViewInitOptions);

	//Pass on the previous view transform from the view info (probably provided by the camera if set)
	View->PreviousViewTransform = ViewInfo.PreviousViewTransform;

	ViewFamily->Views.Add(View);

	{
		View->StartFinalPostprocessSettings(ViewInfo.Location);

		//Post Process Setting
		//TODO?

		//	CAMERA OVERRIDE
		View->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);
		CameraManager->ApplyExtraPostProcessBlends(View);
		FCameraPhotographyManager::Get().UpdatePostProcessing(View->FinalPostProcessSettings);

		if (GEngine->StereoRenderingDevice.IsValid())
		{
			FPostProcessSettings StereoDeviceOverridePostProcessingSettings;
			float BlendWeight = 1.0f;

			const bool StereoSettingsAvailable = GEngine->StereoRenderingDevice->OverrideFinalPostprocessSettings(&StereoDeviceOverridePostProcessingSettings
				, View->StereoPass
				, View->StereoViewIndex
				, BlendWeight);

			if (StereoSettingsAvailable)
			{
				View->OverridePostProcessSettings(StereoDeviceOverridePostProcessingSettings, BlendWeight);
			}
		}


		View->EndFinalPostprocessSettings(ViewInitOptions);
	}

	for (int32 ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ++ViewExt)
	{
		ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
	}
	
	return View;
}
