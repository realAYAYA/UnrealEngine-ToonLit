// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/Effects/BitCrusher.h"
#include "HarmonixDsp/StridePointer.h"

#include "HAL/PlatformMemory.h"
#include "Math/UnrealMath.h"
#include "Math/NumericLimits.h"

namespace Harmonix::Dsp::Effects
{
	FBitCrusher::FBitCrusher()
		: MaxChannels(0)
		, SampleHoldFactor(0)
	{
		SetCrush(0);
		SetWetGain(0.0f, true);
		SetInputGain(1.0f, true);
		SetOutputGain(1.0f, true);
	}

	FBitCrusher::~FBitCrusher()
	{
	}

	void FBitCrusher::Setup(int32 InMaxChannels, float InSampleRate)
	{
		SampleRate = (uint32)InSampleRate;
		WetGainRamper.SetRampTimeMs(SampleRate, 2.0f);
		InputGainRamper.SetRampTimeMs(SampleRate, 2.0f);
		OutputGainRamper.SetRampTimeMs(SampleRate, 2.0f);
		MaxChannels = InMaxChannels;
		Reset();
	}

	void FBitCrusher::Reset()
	{
		SampleHoldStage = 0;
		FMemory::Memset(SampleHoldData, 0, sizeof(SampleHoldData));
		WetGainRamper.SnapToTarget();
		InputGainRamper.SnapToTarget();
		OutputGainRamper.SnapToTarget();
	}

	void FBitCrusher::SetCrush(uint16 InCrushLevel)
	{
		CrushCoef = FMath::Clamp(InCrushLevel, 0, 15);
	}

	void FBitCrusher::SetInputGain(float Gain, bool InSnap)
	{
		InputGainRamper.SetTarget(FMath::Clamp(Gain, 0.0f, 1.0f));
		if (InSnap)
		{
			InputGainRamper.SnapToTarget();
		}
	}

	void FBitCrusher::SetOutputGain(float Gain, bool InSnap)
	{
		OutputGainRamper.SetTarget(FMath::Clamp(Gain, 0.0f, 1.0f));
		if (InSnap)
		{
			OutputGainRamper.SnapToTarget();
		}
	}

	void FBitCrusher::SetWetGain(float Gain, bool InSnap)
	{
		WetGainRamper.SetTarget(FMath::Clamp(Gain, 0.0f, 1.0f));
		if (InSnap)
		{
			WetGainRamper.SnapToTarget();
		}
	}

	void FBitCrusher::SetSampleHoldFactor(uint16 InSampleHoldFactor)
	{
		SampleHoldFactor = InSampleHoldFactor;
	}

	void FBitCrusher::Process(TAudioBuffer<float>& InBuffer, TAudioBuffer<float>& OutBuffer)
	{
		int32 ActiveChannels = InBuffer.GetNumValidChannels();
		ensureAlways(ActiveChannels <= kMaxNumChannels);

		ActiveChannels = FMath::Min(ActiveChannels, kMaxNumChannels);

		const uint32 NumFramesToProcess = InBuffer.GetNumValidFrames();
		checkSlow((NumFramesToProcess % 4) == 0);
		static const float FloatToInt = static_cast<float>(TNumericLimits<int16>::Max());
		static const float IntToFloat = 1.0f / (FloatToInt + 1.0f);

		// temporary ramper to handle multiple channels
		TLinearRamper<float> ChWetGainRamper;
		TLinearRamper<float> ChInputGainRamper;
		TLinearRamper<float> ChOutputGainRamper;

		uint16 ChSampleHoldStage = 0;
		uint16 ChSampleHoldFactor = SampleHoldFactor;

		for (int32 ChIdx = 0; ChIdx < ActiveChannels; ++ChIdx)
		{
			ChSampleHoldStage = SampleHoldStage;

			// set up data for this channel
			TDynamicStridePtr<float> InputData = InBuffer.GetStridingChannelDataPointer(ChIdx);
			TDynamicStridePtr<float> OutputData = OutBuffer.GetStridingChannelDataPointer(ChIdx);

			ChWetGainRamper = WetGainRamper;
			ChInputGainRamper = InputGainRamper;
			ChOutputGainRamper = OutputGainRamper;

			for (uint32 FrameNum = 0; FrameNum < NumFramesToProcess; ++FrameNum)
			{
				float ChWet = ChWetGainRamper.GetCurrent();
				float ChDry = 1.0f - ChWet;

				// dry
				float InputSample = FMath::Clamp(*InputData * ChInputGainRamper.GetCurrent(), -1.0f, 1.0f);

				float CrushedSample = 0.0f;

				if (ChSampleHoldStage == 0)
				{
					// bit crush this sample
					int16 Sample = (int16)(InputSample * FloatToInt);
					Sample = (Sample >> CrushCoef) << CrushCoef;
					CrushedSample = ((float)Sample) * IntToFloat * ChOutputGainRamper.GetCurrent();
					SampleHoldData[ChIdx] = CrushedSample;
				}
				else
				{
					CrushedSample = SampleHoldData[ChIdx];
				}

				++ChSampleHoldStage;
				if (ChSampleHoldStage > ChSampleHoldFactor)
				{
					ChSampleHoldStage = 0;
				}

				// process this sample
				*OutputData = ChDry * InputSample;
				*OutputData += ChWet * CrushedSample;

				InputData++;
				OutputData++;

				ChWetGainRamper.Ramp();
				ChInputGainRamper.Ramp();
				ChOutputGainRamper.Ramp();
			}
		}

		if (ChOutputGainRamper.GetCurrent() > 1.0f)
		{
			OutBuffer.Saturate(-1.0f, 1.0f);
		}

		SampleHoldStage = ChSampleHoldStage;
		WetGainRamper = ChWetGainRamper;
		InputGainRamper = ChInputGainRamper;
		OutputGainRamper = ChOutputGainRamper;
	}

}