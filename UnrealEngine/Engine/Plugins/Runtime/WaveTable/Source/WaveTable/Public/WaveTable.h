// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "HAL/Platform.h"


namespace WaveTable
{
	struct WAVETABLE_API FWaveTable
	{
	private:
		TArray<float> Samples;

	public:
		FWaveTable() = default;

		FWaveTable(const TArray<float>& InSamples)
			: Samples(InSamples)
		{
		}

		FWaveTable(TArray<float>&& InSamples)
			: Samples(MoveTemp(InSamples))
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

		TArrayView<float> GetView()
		{
			return Samples;
		}

		TArrayView<const float> GetView() const
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