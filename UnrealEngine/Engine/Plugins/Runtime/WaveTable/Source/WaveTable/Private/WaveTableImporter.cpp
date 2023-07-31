// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableImporter.h"

#include "DSP/FloatArrayMath.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "WaveTableSampler.h"


namespace WaveTable
{
	FImporter::FImporter(const FWaveTableSettings& InSettings, bool bInBipolar)
		: Settings(InSettings)
		, bBipolar(bInBipolar)
	{
		Sampler.SetInterpolationMode(FWaveTableSampler::EInterpolationMode::Cubic);
		Sampler.SetPhase(Settings.Phase);
	}

	void FImporter::Process(TArray<float>& OutWaveTable)
	{
		// 1. Get editable section of source PCM
		TArrayView<const float> EditSourceView = Settings.GetEditSourceView();
		if (!EditSourceView.IsEmpty())
		{
			// 2. Resample into table
			Sampler.Reset();
			Sampler.Process(EditSourceView, OutWaveTable);

			// 3. Apply offset
			float TableOffset = 0.0f;
			if (Settings.bRemoveOffset || !bBipolar)
			{
				TableOffset = Audio::ArrayGetAverageValue(OutWaveTable);
				Audio::ArrayAddConstantInplace(OutWaveTable, -1.0f * TableOffset);
			}

			// 4. Normalize
			if (Settings.bNormalize)
			{
				const float MaxValue = Audio::ArrayMaxAbsValue(OutWaveTable);
				if (MaxValue > 0.0f)
				{
					Audio::ArrayMultiplyByConstantInPlace(OutWaveTable, 1.0f / MaxValue);
				}
			}

			// 5. Apply fades
			if (Settings.FadeIn > 0.0f)
			{
				const int32 FadeLength = RatioToIndex(Settings.FadeIn, OutWaveTable.Num());
				TArrayView<float> FadeView(OutWaveTable.GetData(), FadeLength);
				Audio::ArrayFade(FadeView, 0.0f, 1.0f);
			}

			if (Settings.FadeOut > 0.0f)
			{
				const int32 FadeLastIndex = RatioToIndex(Settings.FadeOut, OutWaveTable.Num());
				const int32 FadeInitIndex = RatioToIndex(1.0f - Settings.FadeOut, OutWaveTable.Num());
				TArrayView<float> FadeView = TArrayView<float>(&OutWaveTable[FadeInitIndex], FadeLastIndex + 1);
				Audio::ArrayFade(FadeView, 1.0f, 0.0f);
			}

			// 6. Finalize if unipolar source
			if (!bBipolar)
			{
				Audio::ArrayAbsInPlace(OutWaveTable);

				// If not requesting offset removal, add back (is always removed above for
				// unipolar to ensure fades are centered around offset)
				if (!Settings.bRemoveOffset)
				{
					Audio::ArrayAddConstantInplace(OutWaveTable, TableOffset);
				}
			}
		}
	} // namespace Importer
} // namespace WaveTable
