// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableSampler.h"

#include "DSP/Dsp.h"
#include "DSP/FloatArrayMath.h"


namespace WaveTable
{
	namespace UtilitiesPrivate
	{
		UE_NODISCARD FORCEINLINE FVector2f GetTangentP0(const float* InTableView, const float NextValue, float InIndex, int32 InArraySize)
		{
			const int32 Index = FMath::TruncToInt32(InIndex);

			// Addition of array size guarantees that if index is 0, we aren't modding a negative number and returning a negative index
			const float LastValue = InTableView[(Index - 1 + InArraySize) % InArraySize];
			return FVector2f(2.0f, NextValue - LastValue).GetSafeNormal();
		};

		UE_NODISCARD FORCEINLINE FVector2f GetTangentP1(const float* InTableView, const float LastValue, float InIndex, int32 InArraySize)
		{
			const int32 Index = FMath::TruncToInt32(InIndex);
			const float NextValue = InTableView[(Index + 1) % InArraySize];
			return FVector2f(2.0f, NextValue - LastValue).GetSafeNormal();
		};

		auto MaxValueIndexInterpolator = [](TArrayView<const float> InTableView, TArrayView<float> InOutIndicesToValues)
		{
			int32 LastIndex = 0;
			const float* TableData = InTableView.GetData();
			float* IndexValueData = InOutIndicesToValues.GetData();
			for (int32 i = 0; i < InOutIndicesToValues.Num(); ++i)
			{
				int32 Index = FMath::TruncToInt32(InTableView.Num() * IndexValueData[i]) % InTableView.Num();
				bool bSwapped = false;
				if (Index < LastIndex)
				{
					bSwapped = true;
					::Swap(LastIndex, Index);
				}

				int32 MaxAbs = 0.0f;
				for (int32 j = LastIndex; j <= Index; ++j)
				{
					float IndexAbs = FMath::Abs(TableData[j]);
					if (IndexAbs > MaxAbs)
					{
						MaxAbs = IndexAbs;
						IndexValueData[i] = TableData[j];
					}
				}

				if (!bSwapped)
				{
					LastIndex = Index;
				}
			}
		};

		auto CubicIndexInterpolator = [](TArrayView<const float> InTableView, TArrayView<float> InOutIndicesToValues)
		{
			const int32 NumTableSamples = InTableView.Num();
			const float* InTable = InTableView.GetData();
			for (int32 i = 0; i < InOutIndicesToValues.Num(); ++i)
			{
				float& IndexToOutput = InOutIndicesToValues[i];
				const int32 LastIndexInt = FMath::TruncToInt32(IndexToOutput);

				const FVector2f P0 = { (float)LastIndexInt, InTable[LastIndexInt % NumTableSamples] };
				const FVector2f P1 = { (float)(LastIndexInt + 1), InTable[(LastIndexInt + 1) % NumTableSamples] };
				const FVector2f TangentP0 = GetTangentP0(InTable, P1.Y, IndexToOutput, NumTableSamples);
				const FVector2f TangentP1 = GetTangentP1(InTable, P0.Y, IndexToOutput, NumTableSamples);
				const float Alpha = IndexToOutput - LastIndexInt;

				IndexToOutput = FMath::CubicInterp(P0, TangentP0, P1, TangentP1, Alpha).Y;
			}
		};

		auto LinearIndexInterpolator = [](TArrayView<const float> InTableView, const TArrayView<float> InOutIndicesToValues)
		{
			const int32 NumTableSamples = InTableView.Num();
			const float* InTable = InTableView.GetData();
			for (int32 i = 0; i < InOutIndicesToValues.Num(); ++i)
			{
				float& IndexToOutput = InOutIndicesToValues[i];
				const int32 LastIndexInt = FMath::TruncToInt32(IndexToOutput);

				const float P0 = InTable[LastIndexInt % NumTableSamples];
				const float Alpha = IndexToOutput - LastIndexInt;
				const float P1 = InTable[(LastIndexInt + 1) % NumTableSamples];

				IndexToOutput = FMath::Lerp(P0, P1, Alpha);
			}
		};

		auto StepIndexInterpolator = [](TArrayView<const float> InTableView, const TArrayView<float> InOutIndicesToValues)
		{
			const int32 NumTableSamples = InTableView.Num();
			const float* InTable = InTableView.GetData();
			float* IndexToValue = InOutIndicesToValues.GetData();
			for (int32 i = 0; i < InOutIndicesToValues.Num(); ++i)
			{
				float& IndexToOutput = IndexToValue[i];
				const int32 LastIndexInt = FMath::TruncToInt32(IndexToOutput);
				IndexToOutput = InTable[LastIndexInt % NumTableSamples];
			}
		};
	}

	FWaveTableSampler::FWaveTableSampler()
	{
	}

	FWaveTableSampler::FWaveTableSampler(FSettings&& InSettings)
		: Settings(MoveTemp(InSettings))
	{
	}

	void FWaveTableSampler::Interpolate(TArrayView<const float> InTableView, TArrayView<float> InOutIndexToSamplesView, EInterpolationMode InterpMode)
	{
		using namespace UtilitiesPrivate;

		switch (InterpMode)
		{
			case EInterpolationMode::None:
			{
				StepIndexInterpolator(InTableView, InOutIndexToSamplesView);
			}
			break;

			case EInterpolationMode::Linear:
			{
				LinearIndexInterpolator(InTableView, InOutIndexToSamplesView);
			}
			break;

			case EInterpolationMode::Cubic:
			{
				CubicIndexInterpolator(InTableView, InOutIndexToSamplesView);
			}
			break;

			case EInterpolationMode::MaxValue:
			{
				MaxValueIndexInterpolator(InTableView, InOutIndexToSamplesView);
			}
			break;

			default:
			{
				static_assert(static_cast<int32>(EInterpolationMode::COUNT) == 4, "Possible missing switch coverage for EInterpolationMode");
				checkNoEntry();
			}
			break;
		}
	}

	void FWaveTableSampler::Reset()
	{
		LastIndex = 0.0f;
	}

	const FWaveTableSampler::FSettings& FWaveTableSampler::GetSettings() const
	{
		return Settings;
	}

	void FWaveTableSampler::SetFreq(float InFreq)
	{
		Settings.Freq = InFreq;
	}

	void FWaveTableSampler::SetInterpolationMode(EInterpolationMode InMode)
	{
		Settings.InterpolationMode = InMode;
	}

	void FWaveTableSampler::SetPhase(float InPhase)
	{
		Settings.Phase = InPhase;
	}

	float FWaveTableSampler::Process(TArrayView<const float> InTableView, float& OutSample, ESingleSampleMode InMode)
	{
		TArrayView<float> OutSamplesView { &OutSample, 1 };
		float Index = Process(InTableView, { }, { }, { }, OutSamplesView);

		// Sampler functions by default cyclically and interpolates against initial value so
		// early out as following index check and potential interp re-evaluation isn't necessary.
		if (InMode == ESingleSampleMode::Loop)
		{
			return Index;
		}

		// If interpolating between last value and next, check
		// to see if interpolation needs to be re-evaluated.
		// This check is expensive in the context of buffer
		// eval and thus only supported in "single-sample" mode.
		const bool bIsFinalInterp = Index > InTableView.Num() - 1;
		const bool bHasCompleted = Index == 0 && (Settings.Phase > 0.0f || LastIndex > 0);
		if (bIsFinalInterp || bHasCompleted)
		{
			switch (InMode)
			{
				case ESingleSampleMode::Hold:
				{
					OutSample = InTableView.Last();

				}
				break;

				case ESingleSampleMode::Zero:
				case ESingleSampleMode::Unit:
				{
					const float ModeValue = static_cast<float>(InMode);
					if (bIsFinalInterp)
					{
						TArray<float> FinalInterp = { InTableView.Last(), ModeValue };
						OutSample = FMath::Frac(Index);
						Interpolate(FinalInterp, OutSamplesView, Settings.InterpolationMode);
					}
					else
					{
						OutSample = ModeValue;
					}
				}
				break;

				case ESingleSampleMode::Loop:
				{
					checkNoEntry(); // Should be handled above
				}
				break;

				default:
				break;
			}

			if (!FMath::IsNearlyEqual(Settings.Amplitude, 1.0f))
			{
				OutSample *= Settings.Amplitude;
			}

			if (!FMath::IsNearlyZero(Settings.Offset))
			{
				OutSample += Settings.Offset;
			}
		}

		return Index;
	}

	float FWaveTableSampler::Process(TArrayView<const float> InTableView, TArrayView<float> OutSamplesView)
	{
		return Process(InTableView, { }, { }, { }, OutSamplesView);
	}

	float FWaveTableSampler::Process(TArrayView<const float> InTableView, TArrayView<const float> InFreqModulator, TArrayView<const float> InPhaseModulator, TArrayView<const float> InSyncTriggers, TArrayView<float> OutSamplesView)
	{
		float CurViewLastIndex = 0.0f;

		if (!OutSamplesView.IsEmpty())
		{
			if (InTableView.IsEmpty())
			{
				Audio::ArraySetToConstantInplace(OutSamplesView, 0.0f);
			}
			else
			{
				// Compute index frequency & phase and stuff into samples
				// view. Interpolator will convert to associated values.
				ComputeIndexFrequency(InTableView, InFreqModulator, InSyncTriggers, OutSamplesView);
				ComputeIndexPhase(InTableView, InPhaseModulator, OutSamplesView);

				// Capture last index and return in case caller is interested in progress/phase through table (ex. for enveloping).
				CurViewLastIndex = OutSamplesView.Last();

				Interpolate(InTableView, OutSamplesView, Settings.InterpolationMode);
			}

			if (!FMath::IsNearlyEqual(Settings.Amplitude, 1.0f))
			{
				Audio::ArrayMultiplyByConstantInPlace(OutSamplesView, Settings.Amplitude);
			}

			if (!FMath::IsNearlyZero(Settings.Offset))
			{
				Audio::ArrayAddConstantInplace(OutSamplesView, Settings.Offset);
			}
		}

		return CurViewLastIndex;
	}

	void FWaveTableSampler::ComputeIndexFrequency(TArrayView<const float> InTableView, TArrayView<const float> InFreqModulator, TArrayView<const float> InSyncTriggers, TArrayView<float> OutIndicesView)
	{
		Audio::ArraySetToConstantInplace(OutIndicesView, Settings.Freq * InTableView.Num() / OutIndicesView.Num());

		if (InFreqModulator.Num() == OutIndicesView.Num())
		{
			Audio::ArrayMultiplyInPlace(InFreqModulator, OutIndicesView);
		}

		auto TransformLastIndex = [this, InTableView, &InFreqModulator, &InSyncTriggers, &OutIndicesView](float InputIndex)
		{
			if (InFreqModulator.Num() == OutIndicesView.Num())
			{
				return (InFreqModulator.Last() * Settings.Freq * InTableView.Num()) + InputIndex;
			}
			else
			{
				checkf(InFreqModulator.IsEmpty(), TEXT("FreqModulator view should be the same size as the sample view or not supplied (size of 0)."));
				return (Settings.Freq * InTableView.Num()) + InputIndex;
			}
		};

		float* OutSamples = OutIndicesView.GetData();
		if (InSyncTriggers.Num() == OutIndicesView.Num())
		{
			float SyncIndex = 0.0f;
			const float* SyncTrigData = InSyncTriggers.GetData();
			for (int32 i = 0; i < OutIndicesView.Num(); ++i)
			{
				// Numerically negate trig value as high (1.0f) should
				// be zero to "reset" index state (avoids conditional
				// in loop at expense of memory).
				const float InvertedTrig = -1 * SyncTrigData[i] + 1;
				SyncIndex *= InvertedTrig;
				LastIndex *= InvertedTrig;
				OutSamples[i] *= SyncIndex;
				OutSamples[i] += LastIndex;

				SyncIndex++;
			}

			LastIndex = TransformLastIndex(LastIndex) + TransformLastIndex(SyncIndex);
		}
		else
		{
			checkf(InSyncTriggers.IsEmpty(), TEXT("SyncTriggers view should be the same size as the sample view or not supplied (size of 0)."));
			for (int32 i = 0; i < OutIndicesView.Num(); ++i)
			{
				OutSamples[i] *= i;
				OutSamples[i] += LastIndex;
			}

			LastIndex = TransformLastIndex(LastIndex);
		}

		LastIndex = FMath::Frac(LastIndex / InTableView.Num()) * InTableView.Num();
	}

	void FWaveTableSampler::ComputeIndexPhase(TArrayView<const float> InTableView, TArrayView<const float> InPhaseModulator, TArrayView<float> OutIndicesView)
	{
		check(!OutIndicesView.IsEmpty());

		if (InPhaseModulator.Num() == OutIndicesView.Num())
		{
			PhaseModScratch.SetNum(OutIndicesView.Num());
			Audio::ArraySetToConstantInplace(PhaseModScratch, Settings.Phase);
			Audio::ArrayAddInPlace(InPhaseModulator, PhaseModScratch);

			const float* PhaseModData = PhaseModScratch.GetData();
			float* OutSamples = OutIndicesView.GetData();
			for (int32 i = 0; i < OutIndicesView.Num(); ++i)
			{
				const float PhaseIndexOffset = FMath::Frac(PhaseModData[i]) * InTableView.Num();
				OutSamples[i] += PhaseIndexOffset;
			}
		}
		else
		{
			checkf(InPhaseModulator.IsEmpty(), TEXT("PhaseModulator view should be the same size as the sample view or not supplied (size of 0)."));

			const float PhaseIndexOffset = FMath::Frac(Settings.Phase) * InTableView.Num();
			Audio::ArrayAddConstantInplace(OutIndicesView, PhaseIndexOffset);
		}
	}
} // namespace WaveTable
