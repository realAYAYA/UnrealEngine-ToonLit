// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixDsp/Ramper.h"

#include "HAL/Platform.h"

namespace Harmonix::Dsp::Effects
{
	class HARMONIXDSP_API FBitCrusher
	{
	public:

		FBitCrusher();
		virtual ~FBitCrusher();

		void SetCrush(uint16 CrushLevel);
		void SetInputGain(float Gain, bool Snap = false);
		void SetOutputGain(float Gain, bool Snap = false);
		void SetWetGain(float Wet, bool Snap = false);
		void SetSampleHoldFactor(uint16 SampleHoldFactor);

		uint16 GetCrush() const { return CrushCoef; }
		uint16 GetSampleHoldFactor() const { return SampleHoldFactor; }
		float GetWetGain() const { return WetGainRamper.GetTarget(); }

		void Setup(int32 MaxChannels, float SampleRate);
		void Reset();
		int32 GetMaxChannels() { return MaxChannels; }
		void Process(TAudioBuffer<float>& Buffer) { Process(Buffer, Buffer); }
		void Process(TAudioBuffer<float>& InBuffer, TAudioBuffer<float>& OutBuffer);

	private:

		int32 MaxChannels;
		uint32 SampleRate;

		static const int32 kMaxNumChannels = 16;
		float SampleHoldData[kMaxNumChannels];

		TLinearRamper<float> WetGainRamper;
		TLinearRamper<float> InputGainRamper;
		TLinearRamper<float> OutputGainRamper;

		uint16 SampleHoldFactor;
		uint16 CrushCoef;
		uint16 SampleHoldStage;
	};
}