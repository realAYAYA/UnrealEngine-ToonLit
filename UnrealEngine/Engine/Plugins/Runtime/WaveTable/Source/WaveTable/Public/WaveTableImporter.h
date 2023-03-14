// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "WaveTableSettings.h"
#include "WaveTableSampler.h"


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
