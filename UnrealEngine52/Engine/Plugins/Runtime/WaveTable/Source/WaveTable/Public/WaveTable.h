// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ArrayView.h"


namespace WaveTable
{
	struct WAVETABLE_API FWaveTableView
	{
		TArrayView<const float> SampleView;
		float FinalValue;

		explicit FWaveTableView(const TArray<float>& InSamples, const float InFinalValue)
			: SampleView(InSamples), FinalValue(InFinalValue) {}
		
		explicit FWaveTableView(const TArrayView<const float>& InSamples, const float InFinalValue)
			: SampleView(InSamples), FinalValue(InFinalValue) {}

		// returns the length of the SampleView array, not including the additional FinalValue
		int32 Num() const
		{
			return SampleView.Num();
		}

		bool IsEmpty() const
		{
			return SampleView.IsEmpty();
		}
	};
	
	struct WAVETABLE_API FWaveTable
	{
	private:
		TArray<float> Samples;
		float FinalValue;

	public:
		FWaveTable() = default;

		FWaveTable(const TArray<float>& InSamples, const float InFinalValue)
			: Samples(InSamples)
			, FinalValue(InFinalValue)
		{
		}

		FWaveTable(TArray<float>&& InSamples, const float InFinalValue)
			: Samples(MoveTemp(InSamples))
			, FinalValue(InFinalValue)
		{
		}

		float* GetData()
		{
			return Samples.GetData();
		}

		const float* GetData() const
		{
			return Samples.GetData();
		}

		FWaveTableView GetView() const
		{
			return FWaveTableView(Samples, FinalValue);
		}

		TArrayView<float> GetSamples()
		{
			return Samples;
		}

		int32 Num() const
		{
			return Samples.Num();
		}

		void Set(TArray<float>&& InSamples)
		{
			Samples = MoveTemp(InSamples);
		}

		void Set(TArrayView<const float> InSamples)
		{
			Samples = InSamples;
		}

		void SetFinalValue(const float InValue)
		{
			FinalValue = InValue;
		}

		void SetNum(int32 InSize)
		{
			Samples.SetNum(InSize);
		}

		void Zero()
		{
			FMemory::Memset(Samples.GetData(), 0, sizeof(float) * Samples.Num());
		}
	};
} // namespace WaveTable
