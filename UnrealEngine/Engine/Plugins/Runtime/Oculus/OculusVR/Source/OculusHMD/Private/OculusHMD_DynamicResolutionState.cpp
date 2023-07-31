// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_DynamicResolutionState.h"
#include "LegacyScreenPercentageDriver.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "SceneView.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FDynamicResolutionState implementation
//-------------------------------------------------------------------------------------------------

FDynamicResolutionState::FDynamicResolutionState(const OculusHMD::FSettingsPtr InSettings)
	: Settings(InSettings)
	, ResolutionFraction(-1.0f)
	, ResolutionFractionUpperBound(-1.0f)
{
	check(Settings.IsValid());
}

void FDynamicResolutionState::ResetHistory()
{
	// Empty - Oculus drives resolution fraction externally
};

bool FDynamicResolutionState::IsSupported() const
{
	return true;
}

void FDynamicResolutionState::SetupMainViewFamily(class FSceneViewFamily& ViewFamily)
{
	check(IsInGameThread());
	check(ViewFamily.EngineShowFlags.ScreenPercentage == true);

	if (ViewFamily.Views.Num() > 0 && IsEnabled())
	{
		// We can assume both eyes have the same fraction
		const FSceneView& View = *ViewFamily.Views[0];
		check(View.UnconstrainedViewRect == View.UnscaledViewRect);

		// Compute desired resolution fraction range
		float MinResolutionFraction = Settings->PixelDensityMin;
		float MaxResolutionFraction = Settings->PixelDensityMax;

		// Clamp resolution fraction to what the renderer can do.
		MinResolutionFraction = FMath::Max(MinResolutionFraction, ISceneViewFamilyScreenPercentage::kMinResolutionFraction);
		MaxResolutionFraction = FMath::Min(MaxResolutionFraction, ISceneViewFamilyScreenPercentage::kMaxResolutionFraction);

		if (View.AntiAliasingMethod == AAM_TSR)
		{
			MinResolutionFraction = FMath::Max(MinResolutionFraction, ISceneViewFamilyScreenPercentage::kMinTSRResolutionFraction);
			MaxResolutionFraction = FMath::Min(MaxResolutionFraction, ISceneViewFamilyScreenPercentage::kMaxTSRResolutionFraction);
		}
		else if (View.AntiAliasingMethod == AAM_TemporalAA)
		{
			MinResolutionFraction = FMath::Max(MinResolutionFraction, ISceneViewFamilyScreenPercentage::kMinTAAUpsampleResolutionFraction);
			MaxResolutionFraction = FMath::Min(MaxResolutionFraction, ISceneViewFamilyScreenPercentage::kMaxTAAUpsampleResolutionFraction);
		}

		ResolutionFraction = FMath::Clamp(Settings->PixelDensity, MinResolutionFraction, MaxResolutionFraction);
		ResolutionFractionUpperBound = MaxResolutionFraction;

		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, ResolutionFraction, ResolutionFractionUpperBound));
	}
}

DynamicRenderScaling::TMap<float> FDynamicResolutionState::GetResolutionFractionsApproximation() const
{
	DynamicRenderScaling::TMap<float> ResolutionFractions;
	ResolutionFractions.SetAll(1.0f);
	ResolutionFractions[GDynamicPrimaryResolutionFraction] = ResolutionFraction;
	return ResolutionFractions;
}

DynamicRenderScaling::TMap<float> FDynamicResolutionState::GetResolutionFractionsUpperBound() const
{
	DynamicRenderScaling::TMap<float> ResolutionFractions;
	ResolutionFractions.SetAll(1.0f);
	ResolutionFractions[GDynamicPrimaryResolutionFraction] = ResolutionFractionUpperBound;
	return ResolutionFractions;
}

void FDynamicResolutionState::SetEnabled(bool bEnable)
{
	check(IsInGameThread());
	Settings->Flags.bPixelDensityAdaptive = bEnable;
}

bool FDynamicResolutionState::IsEnabled() const
{
	check(IsInGameThread());
	return Settings->Flags.bPixelDensityAdaptive;
}

void FDynamicResolutionState::ProcessEvent(EDynamicResolutionStateEvent Event)
{
	// Empty - Oculus drives resolution fraction externally
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
