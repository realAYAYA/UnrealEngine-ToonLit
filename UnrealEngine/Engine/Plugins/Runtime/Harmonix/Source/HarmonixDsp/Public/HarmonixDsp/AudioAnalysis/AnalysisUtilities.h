// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FHarmonixAudioAnalyzerOutputSettings;

namespace Harmonix::Dsp::AudioAnalysis
{
	/**
	 * @brief Smooth a value based on some input settings
	 * @param TargetEnergyLinear - The current target should be an energy rather than amplitude (squared and non-negative)
	 * @param PreviousEnergyLinear - The previous energy
	 * @param ElapsedMs - The amount of time elapsed since the last call to this function
	 * @param Settings - The settings to use for the scaling and smoothing
	 * @return The smoothed value
	 */
	float SmoothEnergy(
		float TargetEnergyLinear,
		float PreviousEnergyLinear,
		float ElapsedMs,
		const FHarmonixAudioAnalyzerOutputSettings& Settings);
}
