// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSceneViewExtension.h"
#include "DynamicResolutionState.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SceneView.h"

#define LOCTEXT_NAMESPACE "ImgMediaSceneViewExtension"

static TAutoConsoleVariable<float> CVarImgMediaFieldOfViewMultiplier(
	TEXT("ImgMedia.FieldOfViewMultiplier"),
	1.0f,
	TEXT("Multiply the field of view for active cameras by this value, generally to increase the frustum overall sizes to mitigate missing tile artifacts.\n"),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarImgMediaProcessTilesInnerOnly(
	TEXT("ImgMedia.ICVFX.InnerOnlyTiles"),
	false,
	TEXT("This CVar will ignore tile calculation for all viewports except for Display Cluster inner viewports. User should enable upscaling on Media plate to display lower quality mips instead, otherwise ")
	TEXT("other viewports will only display tiles loaded specifically for inner viewport and nothing else. \n"),
#if WITH_EDITOR
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar)
	{
		if (CVar->GetBool())
		{
			FNotificationInfo Info(LOCTEXT("EnableUpscalingNotification", "Tile calculation enabled for Display Cluster Inner Viewports exclusively.\nUse Mip Upscaling option on Media Plate to fill empty texture areas with lower quality data."));
			// Expire in 5 seconds.
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}),
#endif
	ECVF_Default);


FImgMediaSceneViewExtension::FImgMediaSceneViewExtension(const FAutoRegister& AutoReg)
	: FSceneViewExtensionBase(AutoReg)
	, CachedViewInfos()
	, LastFrameNumber(0)
{
}

void FImgMediaSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FImgMediaSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FImgMediaSceneViewExtension::SetupView);

	/**
	* NOTE: Scene captures call `SetupView` after the primary `BeginRenderViewFamily` call:
	* FRendererModule::BeginRenderingViewFamilies
	*     -> USceneCaptureComponent::UpdateDeferredCaptures
	*         -> FScene::UpdateSceneCaptureContents
	*             -> ISceneViewExtension::SetupView
	* Therefore, the view infos we cache here will be correctly kept until the next frame.
	*/
	if (InView.bIsSceneCapture)
	{
		CacheViewInfo(InViewFamily, InView);
	}
}

void FImgMediaSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FImgMediaSceneViewExtension::BeginRenderViewFamily);

	if (LastFrameNumber != InViewFamily.FrameNumber)
	{
		CachedViewInfos.Reset();
		LastFrameNumber = InViewFamily.FrameNumber;
	}

	for (const FSceneView* View : InViewFamily.Views)
	{
		/** NOTE: Scene captures currently don't call BeginRenderViewFamily() so we handle them separately in SetupView. */
		if (View && !View->bIsSceneCapture)
		{
			CacheViewInfo(InViewFamily, *View);
		}
	}
}

int32 FImgMediaSceneViewExtension::GetPriority() const
{
	// Lowest priority value to ensure all other extensions are executed before ours.
	return MIN_int32;
}

void FImgMediaSceneViewExtension::CacheViewInfo(FSceneViewFamily& InViewFamily, const FSceneView& View)
{
	// This relies on DisplayClusterMediaHelpers::GenerateICVFXViewportName to have two strings embedded.
	if (CVarImgMediaProcessTilesInnerOnly.GetValueOnGameThread()
		&& !(InViewFamily.ProfileDescription.Contains("_icvfx_") && InViewFamily.ProfileDescription.Contains("_incamera")))
	{
		return;
	}
	static const auto CVarMinAutomaticViewMipBiasOffset = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ViewTextureMipBias.Offset"));
	static const auto CVarMinAutomaticViewMipBias = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ViewTextureMipBias.Min"));
	const float FieldOfViewMultiplier = CVarImgMediaFieldOfViewMultiplier.GetValueOnGameThread();

	float ResolutionFraction = InViewFamily.SecondaryViewFraction;

	if (InViewFamily.GetScreenPercentageInterface())
	{
		DynamicRenderScaling::TMap<float> UpperBounds = InViewFamily.GetScreenPercentageInterface()->GetResolutionFractionsUpperBound();
		ResolutionFraction *= UpperBounds[GDynamicPrimaryResolutionFraction];
	}

	FImgMediaViewInfo Info;
	Info.Location = View.ViewMatrices.GetViewOrigin();
	Info.ViewDirection = View.GetViewDirection();
	Info.ViewProjectionMatrix = View.ViewMatrices.GetViewProjectionMatrix();

	if (FMath::IsNearlyEqual(FieldOfViewMultiplier, 1.0f))
	{
		Info.OverscanViewProjectionMatrix = Info.ViewProjectionMatrix;
	}
	else
	{
		FMatrix AdjustedProjectionMatrix = View.ViewMatrices.GetProjectionMatrix();

		const double HalfHorizontalFOV = FMath::Atan(1.0 / AdjustedProjectionMatrix.M[0][0]);
		const double HalfVerticalFOV = FMath::Atan(1.0 / AdjustedProjectionMatrix.M[1][1]);

		AdjustedProjectionMatrix.M[0][0] = 1.0 / FMath::Tan(HalfHorizontalFOV * FieldOfViewMultiplier);
		AdjustedProjectionMatrix.M[1][1] = 1.0 / FMath::Tan(HalfVerticalFOV * FieldOfViewMultiplier);

		Info.OverscanViewProjectionMatrix = View.ViewMatrices.GetViewMatrix() * AdjustedProjectionMatrix;
	}

	Info.ViewportRect = View.UnconstrainedViewRect.Scale(ResolutionFraction);

	// We store hidden or show-only ids to later avoid needless calculations when objects are not in view.
	if (View.ShowOnlyPrimitives.IsSet())
	{
		Info.bPrimitiveHiddenMode = false;
		Info.PrimitiveComponentIds = View.ShowOnlyPrimitives.GetValue();
	}
	else
	{
		Info.bPrimitiveHiddenMode = true;
		Info.PrimitiveComponentIds = View.HiddenPrimitives;
	}

	/* View.MaterialTextureMipBias is only set later in rendering so we replicate here the calculations
	 * found in FSceneRenderer::PreVisibilityFrameSetup.*/
	if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		const float EffectivePrimaryResolutionFraction = float(Info.ViewportRect.Width()) / (View.UnscaledViewRect.Width() * InViewFamily.SecondaryViewFraction);
		Info.MaterialTextureMipBias = -(FMath::Max(-FMath::Log2(EffectivePrimaryResolutionFraction), 0.0f)) + CVarMinAutomaticViewMipBiasOffset->GetValueOnGameThread();
		Info.MaterialTextureMipBias = FMath::Max(Info.MaterialTextureMipBias, CVarMinAutomaticViewMipBias->GetValueOnGameThread());

		if (!ensureMsgf(!FMath::IsNaN(Info.MaterialTextureMipBias) && FMath::IsFinite(Info.MaterialTextureMipBias), TEXT("Calculated material texture mip bias is invalid, defaulting to zero.")))
		{
			Info.MaterialTextureMipBias = 0.0f;
		}
	}
	else
	{
		Info.MaterialTextureMipBias = 0.0f;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto CVarMipMapDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("ImgMedia.MipMapDebug"));

	if (GEngine != nullptr && CVarMipMapDebug != nullptr && CVarMipMapDebug->GetBool())
	{
		const FString ViewName = InViewFamily.ProfileDescription.IsEmpty() ? TEXT("View") : InViewFamily.ProfileDescription;
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, *FString::Printf(TEXT("%s location: [%s], direction: [%s]"), *ViewName, *Info.Location.ToString(), *View.GetViewDirection().ToString()));
	}
#endif

	CachedViewInfos.Add(MoveTemp(Info));
}
#undef LOCTEXT_NAMESPACE
