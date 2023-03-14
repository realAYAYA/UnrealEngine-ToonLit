// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RendererPrivate.h: Renderer interface private definitions.
=============================================================================*/

#pragma once

#include "SceneView.h"


/**
 * Default screen percentage interface that just apply View->FinalPostProcessSettings.ScreenPercentage.
 */
class ENGINE_API FLegacyScreenPercentageDriver : public ISceneViewFamilyScreenPercentage
{
public:
	FORCEINLINE FLegacyScreenPercentageDriver(
		const FSceneViewFamily& InViewFamily,
		float InGlobalResolutionFraction)
		: FLegacyScreenPercentageDriver(InViewFamily, InGlobalResolutionFraction, InGlobalResolutionFraction)
	{ }

	FLegacyScreenPercentageDriver(
		const FSceneViewFamily& InViewFamily,
		float InGlobalResolutionFraction,
		float InGlobalResolutionFractionUpperBound);

	/** Gets the view rect fraction from the r.ScreenPercentage cvar. */
	static float GetCVarResolutionFraction();

private:
	// View family to take care of.
	const FSceneViewFamily& ViewFamily;

	// ViewRect fraction to apply to all view of the view family.
	const float GlobalResolutionFraction;

	// ViewRect fraction to apply to all view of the view family.
	const float GlobalResolutionFractionUpperBound;


	// Implements ISceneViewFamilyScreenPercentage
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractionsUpperBound() const override;
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractions_RenderThread() const override;
	virtual ISceneViewFamilyScreenPercentage* Fork_GameThread(const class FSceneViewFamily& ForkedViewFamily) const override;
};

// Mode for the computation of the screen percentage (r.ScreenPercentage.Mode).
UENUM()
enum class EScreenPercentageMode
{
	// Directly controls the screen percentage with the r.ScreenPercentage cvar
	Manual UMETA(DisplayName="Manual"),

	// Automatic control the screen percentage based on the display resolution, r.ScreenPercentage.Auto.*
	BasedOnDisplayResolution UMETA(DisplayName="Based on display resolution"),

	// Based on DPI scale.
	BasedOnDPIScale UMETA(DisplayName="Based on operating system's DPI scale"),
};

/**
 * Heuristic to automatically compute a default resolution fraction based user settings and display information.
 */
struct ENGINE_API FStaticResolutionFractionHeuristic
{
	FStaticResolutionFractionHeuristic() = default;

	FStaticResolutionFractionHeuristic(const FEngineShowFlags& EngineShowFlags);

	// User configurable settings
	struct ENGINE_API FUserSettings
	{
		EScreenPercentageMode Mode = EScreenPercentageMode::Manual;

		// r.ScreenPercentage when Mode = EMode::Manual.
		float GlobalResolutionFraction = 1.0f;

		// r.ScreenPercentage.{Min,Max}Resolution
		float MinRenderingResolution = 0.0f;
		float MaxRenderingResolution = 0.0f;

		// r.ScreenPercentage.Auto.* Mode = EMode::BasedOnDisplayResolution.
		float AutoPixelCountMultiplier = 1.0f;

		// stereo HMDs cannot use percentage modes based on 2D monitor
		bool bAllowDisplayBasedScreenPercentageMode = true;

		/** Return whether should use the editor settings for PIE. */
#if WITH_EDITOR
		static bool EditorOverridePIESettings();
#else
		static inline bool EditorOverridePIESettings()
		{
			return false;
		}
#endif

		/** Pulls the user settings from the gameplay runtime cvars. */
		void PullRunTimeRenderingSettings();

		/** Pulls the user settings from the editor cvars. */
		void PullEditorRenderingSettings(bool bIsRealTime, bool bIsPathTraced);
	};

	FUserSettings Settings;

	// [Required] total number of pixel being display into viewport.
	int32 TotalDisplayedPixelCount = 0;

	// [Required] secondary resolution fraction.
	float SecondaryViewFraction = 1.0f;

	// [Required] DPI scale
	float DPIScale = 1.0f;

	// Fetches rendering information from the ViewFamily.
	void PullViewFamilyRenderingSettings(const FSceneViewFamily& ViewFamily);

	float ResolveResolutionFraction() const;
};
