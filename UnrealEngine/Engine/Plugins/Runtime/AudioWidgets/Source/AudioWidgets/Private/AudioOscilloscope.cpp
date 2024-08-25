// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioOscilloscope.h"

namespace AudioWidgets
{
	FAudioOscilloscope::FAudioOscilloscope(Audio::FDeviceId InAudioDeviceId,
		const uint32 InNumChannels, 
		const float InTimeWindowMs, 
		const float InMaxTimeWindowMs, 
		const float InAnalysisPeriodMs, 
		const EAudioPanelLayoutType InPanelLayoutType)
		: OscilloscopePanelStyle(FAudioOscilloscopePanelStyle::GetDefault())
	{
		CreateAudioBus(InNumChannels);
		CreateDataProvider(InAudioDeviceId, InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs, InPanelLayoutType);
		CreateOscilloscopeWidget(InNumChannels, InPanelLayoutType);
	}

	void FAudioOscilloscope::CreateAudioBus(const uint32 InNumChannels)
	{
		AudioBus = TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = AudioBusUtils::ConvertIntToEAudioBusChannels(InNumChannels);
	}

	void FAudioOscilloscope::CreateDataProvider(Audio::FDeviceId InAudioDeviceId,
		const float InTimeWindowMs,
		const float InMaxTimeWindowMs,
		const float InAnalysisPeriodMs,
		const EAudioPanelLayoutType InPanelLayoutType)
	{
		check(AudioBus);

		const uint32 NumChannelsToProvide = (InPanelLayoutType == EAudioPanelLayoutType::Advanced) ? 1 : AudioBus->GetNumChannels(); // Advanced mode waveform display is based on channel selection
		AudioSamplesDataProvider = MakeShared<FWaveformAudioSamplesDataProvider>(InAudioDeviceId, AudioBus.Get(), NumChannelsToProvide, InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs);
	}

	void FAudioOscilloscope::CreateOscilloscopeWidget(const uint32 InNumChannels, const EAudioPanelLayoutType InPanelLayoutType)
	{
		check(AudioSamplesDataProvider);

		const FFixedSampledSequenceView SequenceView = AudioSamplesDataProvider->GetDataView();

		if (!OscilloscopePanelWidget.IsValid())
		{
			OscilloscopePanelWidget = SNew(SAudioOscilloscopePanelWidget, SequenceView, InNumChannels)
			.PanelLayoutType(InPanelLayoutType)
			.PanelStyle(&OscilloscopePanelStyle);
		}
		else
		{
			OscilloscopePanelWidget->BuildWidget(SequenceView, InNumChannels, InPanelLayoutType);
		}

		// Interconnect data provider and widget
		AudioSamplesDataProvider->OnDataViewGenerated.AddSP(OscilloscopePanelWidget.Get(), &SAudioOscilloscopePanelWidget::ReceiveSequenceView);

		if (InPanelLayoutType == EAudioPanelLayoutType::Advanced)
		{
			OscilloscopePanelWidget->OnSelectedChannelChanged.AddSP(AudioSamplesDataProvider.Get(),  &FWaveformAudioSamplesDataProvider::SetChannelToAnalyze);
			OscilloscopePanelWidget->OnTriggerModeChanged.AddSP(AudioSamplesDataProvider.Get(),      &FWaveformAudioSamplesDataProvider::SetTriggerMode);
			OscilloscopePanelWidget->OnTriggerThresholdChanged.AddSP(AudioSamplesDataProvider.Get(), &FWaveformAudioSamplesDataProvider::SetTriggerThreshold);
			OscilloscopePanelWidget->OnTimeWindowValueChanged.AddSP(AudioSamplesDataProvider.Get(),  &FWaveformAudioSamplesDataProvider::SetTimeWindow);
			OscilloscopePanelWidget->OnAnalysisPeriodChanged.AddSP(AudioSamplesDataProvider.Get(),   &FWaveformAudioSamplesDataProvider::SetAnalysisPeriod);
		}
	}

	void FAudioOscilloscope::StartProcessing()
	{
		AudioSamplesDataProvider->StartProcessing();
	}

	void FAudioOscilloscope::StopProcessing()
	{
		AudioSamplesDataProvider->StopProcessing();
	}

	UAudioBus* FAudioOscilloscope::GetAudioBus() const
	{
		return AudioBus.Get();
	}

	TSharedRef<SWidget> FAudioOscilloscope::GetPanelWidget() const
	{
		return OscilloscopePanelWidget.ToSharedRef();
	}
}
