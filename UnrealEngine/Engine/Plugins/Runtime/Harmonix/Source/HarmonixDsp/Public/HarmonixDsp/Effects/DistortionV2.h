// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixDsp/AudioUtility.h"
#include "HarmonixDsp/Ramper.h"
#include "HarmonixDsp/Effects/Settings/DistortionSettings.h"
#include "HarmonixDsp/Effects/BiquadFilter.h"
#include "HarmonixDsp/Effects/FirFilter.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"

namespace Harmonix::Dsp::Effects
{

	class HARMONIXDSP_API FDistortionV2
	{
	public:
		FDistortionV2(uint32 SampleRate = 48000, uint32 MaxRenderBufferSize = 128);
		void Setup(uint32 SampleRate, uint32 MaxRenderBufferSize);
		virtual ~FDistortionV2() {};

		void Process(TAudioBuffer<float>& InOutBuffer) { Process(InOutBuffer, InOutBuffer); }
		void Process(TAudioBuffer<float>& InBuffer, TAudioBuffer<float>& OutBuffer);

		void  SetInputGainDb(float InGainDb, bool Snap = false);
		float GetInputGainDb() const;

		void SetInputGain(float InGain, bool Snap = false);
		float GetInputGain() const { return InputGain; }

		void  SetOutputGainDb(float InGainDb, bool Snap = false);
		float GetOutputGainDb() const;

		void SetOutputGain(float InGain, bool Snap = false);
		float GetOutputGain() const { return OutputGain; }

		void  SetDryGainDb(float InGainDb, bool Snap = false);
		float GetDryGainDb() const;

		void SetDryGain(float InGain, bool Snap = false);
		float GetDryGain() const { return DryGain; }

		void  SetWetGainDb(float InGainDb, bool Snap = false);
		float GetWetGainDb() const;

		void SetWetGain(float InGain, bool Snap = false);
		float GetWetGain() const { return WetGain; }

		void  SetMix(float Mix, bool Snap = false);
		void  SetDCOffset(float Offset, bool Snap = false);

		float GetDCOffset() const { return DCAdjust; }
		void  SetType(uint8 InType);
		void  SetType(EDistortionTypeV2 Type);
		EDistortionTypeV2 GetType() const { return Type; }
		void SetupFilter(int32 Index, const FDistortionFilterSettings& InSettings);
		int32  GetFilterPasses(int32 Index) { return FilterPasses[Index]; }
		void SetOversample(bool Oversample, int32 RenderBufferSizeFrames);
		bool GetOversample() const { return DoOversampling; }
		void SetSampleRate(uint32 SampleRate);

		void Setup(const FDistortionSettingsV2& Settings, uint32 SampleRate, uint32 RenderBufferSizeFrames, bool Snap);

		void Reset();

		static const int32 kMaxChannels = 8;
		static const int32 kMaxFilterPasses = 3;
		static const int32 kRampHops = 16;

	private:
		EDistortionTypeV2	  Type;
		TLinearRamper<float>  InputGain;
		TLinearRamper<float>  OutputGain;
		TLinearRamper<float>  DCAdjust;
		TLinearRamper<float>  DryGain;
		TLinearRamper<float>  WetGain;
		bool		          FilterPreClip[FDistortionSettingsV2::kNumFilters];
		FBiquadFilterSettings FilterSettings[FDistortionSettingsV2::kNumFilters];
		TLinearRamper<float>  FilterGain[FDistortionSettingsV2::kNumFilters];

		TLinearRamper<FBiquadFilterCoefs> FilterCoefs[FDistortionSettingsV2::kNumFilters];

		uint32					FilterPasses[kMaxFilterPasses];

		TMultipassBiquadFilter<double, kMaxFilterPasses> Filter[FDistortionSettingsV2::kNumFilters][kMaxChannels];

		FFirFilter32			OversampleFilterUp[kMaxChannels];
		FFirFilter32			OversampleFilterDown[kMaxChannels];
		bool					DoOversampling;
		TAudioBuffer<float>		UpsampleBuffer;
		uint32					SampleRate;

		static const uint32   kNumFilterTaps = 32;
		static const float    kOversamplingFilterTaps[kNumFilterTaps];

		mutable FCriticalSection SettingsLock;
	};
}