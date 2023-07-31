// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSceneViewExtension.h"
#include "DynamicResolutionState.h"

#include "SceneView.h"

static TAutoConsoleVariable<float> CVarImgMediaFieldOfViewMultiplier(
	TEXT("ImgMedia.FieldOfViewMultiplier"),
	1.0f,
	TEXT("Multiply the field of view for active cameras by this value, generally to increase the frustum overall sizes to mitigate missing tile artifacts.\n"),
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
}

void FImgMediaSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FImgMediaSceneViewExtension::BeginRenderViewFamily);

	if (LastFrameNumber != InViewFamily.FrameNumber)
	{
		CachedViewInfos.Reset();
		LastFrameNumber = InViewFamily.FrameNumber;
	}

	float ResolutionFraction = InViewFamily.SecondaryViewFraction;

	if (InViewFamily.GetScreenPercentageInterface())
	{
		DynamicRenderScaling::TMap<float> UpperBounds = InViewFamily.GetScreenPercentageInterface()->GetResolutionFractionsUpperBound();
		ResolutionFraction *= UpperBounds[GDynamicPrimaryResolutionFraction];
	}

	static const auto CVarMinAutomaticViewMipBiasOffset = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ViewTextureMipBias.Offset"));
	static const auto CVarMinAutomaticViewMipBias = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ViewTextureMipBias.Min"));
	const float FieldOfViewMultiplier = CVarImgMediaFieldOfViewMultiplier.GetValueOnGameThread();

	for (const FSceneView* View : InViewFamily.Views)
	{
		FImgMediaViewInfo Info;
		Info.Location = View->ViewMatrices.GetViewOrigin();
		Info.ViewDirection = View->GetViewDirection();
		Info.ViewProjectionMatrix = View->ViewMatrices.GetViewProjectionMatrix();

		if (FMath::IsNearlyEqual(FieldOfViewMultiplier, 1.0f))
		{
			Info.OverscanViewProjectionMatrix = Info.ViewProjectionMatrix;
		}
		else
		{
			FMatrix AdjustedProjectionMatrix = View->ViewMatrices.GetProjectionMatrix();

			const double HalfHorizontalFOV = FMath::Atan(1.0 / AdjustedProjectionMatrix.M[0][0]);
			const double HalfVerticalFOV = FMath::Atan(1.0 / AdjustedProjectionMatrix.M[1][1]);

			AdjustedProjectionMatrix.M[0][0] = 1.0 / FMath::Tan(HalfHorizontalFOV * FieldOfViewMultiplier);
			AdjustedProjectionMatrix.M[1][1] = 1.0 / FMath::Tan(HalfVerticalFOV * FieldOfViewMultiplier);
			
			Info.OverscanViewProjectionMatrix = View->ViewMatrices.GetViewMatrix() * AdjustedProjectionMatrix;
		}
		
		Info.ViewportRect = View->UnconstrainedViewRect.Scale(ResolutionFraction);

		// We store hidden or show-only ids to later avoid needless calculations when objects are not in view.
		if (View->ShowOnlyPrimitives.IsSet())
		{
			Info.bPrimitiveHiddenMode = false;
			Info.PrimitiveComponentIds = View->ShowOnlyPrimitives.GetValue();
		}
		else
		{
			Info.bPrimitiveHiddenMode = true;
			Info.PrimitiveComponentIds = View->HiddenPrimitives;
		}

		/* View->MaterialTextureMipBias is only set later in rendering so we replicate here the calculations
		 * found in FSceneRenderer::PreVisibilityFrameSetup.*/
		if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
		{
			const float EffectivePrimaryResolutionFraction = float(Info.ViewportRect.Width()) / (View->UnscaledViewRect.Width() * InViewFamily.SecondaryViewFraction);
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
			const FString ViewName = InViewFamily.ProfileDescription.IsEmpty() ? TEXT("View"): InViewFamily.ProfileDescription;
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, *FString::Printf(TEXT("%s location: [%s], direction: [%s]"), *ViewName, *Info.Location.ToString(), *View->GetViewDirection().ToString()));
		}
#endif

		CachedViewInfos.Add(MoveTemp(Info));
	}
}

int32 FImgMediaSceneViewExtension::GetPriority() const
{
	// Lowest priority value to ensure all other extensions are executed before ours.
	return MIN_int32;
}
