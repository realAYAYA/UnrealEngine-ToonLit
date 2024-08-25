// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDefines.h"
#include "AudioOscilloscopeEnums.h"
#include "Containers/ArrayView.h"
#include "Containers/Ticker.h"
#include "DSP/Dsp.h"
#include "DSP/MultithreadedPatching.h"
#include "FixedSampledSequenceView.h"
#include "IFixedSampledSequenceViewProvider.h"

namespace Audio
{
	class FMixerDevice;
}

class UAudioBus;

struct FWaveformAudioSamplesResult;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDataViewGenerated, FFixedSampledSequenceView /*InView*/, const uint32 /*FirstSampleIndex*/)

namespace AudioWidgets
{
	class AUDIOWIDGETS_API FWaveformAudioSamplesDataProvider : public IFixedSampledSequenceViewProvider, 
															   public TSharedFromThis<FWaveformAudioSamplesDataProvider>
	{
	public:
		FWaveformAudioSamplesDataProvider(const Audio::FDeviceId InAudioDeviceId,
			UAudioBus* InAudioBus, 
			const uint32 InNumChannelToProvide, 
			const float InTimeWindowMs, 
			const float InMaxTimeWindowMs, 
			const float InAnalysisPeriodMs);

		virtual ~FWaveformAudioSamplesDataProvider();

		void ResetAudioBuffers();

		void StartProcessing();
		void StopProcessing();

		FFixedSampledSequenceView GetDataView() { return DataView; };
		uint32 GetNumChannels() { return NumChannels; }
		const UAudioBus* GetAudioBus() { return AudioBus; }

		void SetChannelToAnalyze(const int32 InChannel);
		void SetTriggerMode(const EAudioOscilloscopeTriggerMode InTriggerMode);
		void SetTriggerThreshold(const float InTriggerThreshold);
		void SetTimeWindow(const float InTimeWindowMs);
		void SetAnalysisPeriod(const float InAnalysisPeriodMs);

		virtual FFixedSampledSequenceView RequestSequenceView(const TRange<double> DataRatioRange) override;

		FOnDataViewGenerated OnDataViewGenerated;

	private:
		void PushAudioSamplesToCircularBuffer();
		bool Tick(float DeltaTime);

		FTSTicker::FDelegateHandle TickerHandle = nullptr;

		uint32 NumChannelsToProvide = 0;
		uint32 NumChannels          = 0;
		uint32 SampleRate           = 0;

		uint32 TimeWindowMaxTimeSamples = 0;

		const Audio::FMixerDevice* MixerDevice = nullptr;

		UAudioBus* AudioBus = nullptr;
		Audio::FPatchOutputStrongPtr PatchOutput = nullptr;

		TArray<float> TempAudioBuffer;
		Audio::TCircularAudioBuffer<float> AudioSamplesCircularBuffer;
		TArray<float> AudioSamplesForView;
		FFixedSampledSequenceView DataView;

		int32 ChannelIndexToAnalyze = 0;

		EAudioOscilloscopeTriggerMode TriggerMode = EAudioOscilloscopeTriggerMode::None;
		float TriggerThreshold = 0.0f;

		uint32 TimeWindowSamples     = 0;
		uint32 AnalysisPeriodSamples = 0;

		bool bIsProcessing = false;
		bool bHasTriggered = false;

		uint32 NumSamplesPushedToCircularBuffer = 0;
	};
} // namespace AudioWidgets
