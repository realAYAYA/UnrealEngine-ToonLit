// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WaveTableSampler.h"

struct FWaveTableSettings;


namespace WaveTable
{
	class WAVETABLE_API FImporter
	{
	public:
		FImporter(const FWaveTableSettings& InSettings, bool bInBipolar);

		void Process(TArray<float>& OutWaveTable);

	private:
		const FWaveTableSettings& Settings;

		bool bBipolar = false;

		FWaveTableSampler Sampler;
	};
} // namespace WaveTable

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "WaveTableSettings.h"
#endif
