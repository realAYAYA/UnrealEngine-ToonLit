// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDefines.h"
#include "AudioOscilloscopePanelStyle.h"
#include "AudioWidgetsEnums.h"
#include "SAudioOscilloscopePanelWidget.h"
#include "Sound/AudioBus.h"
#include "UObject/StrongObjectPtr.h"
#include "WaveformAudioSamplesDataProvider.h"

class SAudioOscilloscopePanelWidget;

namespace AudioWidgets
{
	class FWaveformAudioSamplesDataProvider;

	class AUDIOWIDGETS_API FAudioOscilloscope
	{
	public:
		FAudioOscilloscope(Audio::FDeviceId InAudioDeviceId,
			const uint32 InNumChannels, 
			const float InTimeWindowMs, 
			const float InMaxTimeWindowMs, 
			const float InAnalysisPeriodMs, 
			const EAudioPanelLayoutType InPanelLayoutType);

		void CreateAudioBus(const uint32 InNumChannels);

		void CreateDataProvider(Audio::FDeviceId InAudioDeviceId,
			const float InTimeWindowMs,
			const float InMaxTimeWindowMs,
			const float InAnalysisPeriodMs,
			const EAudioPanelLayoutType InPanelLayoutType);

		void CreateOscilloscopeWidget(const uint32 InNumChannels, const EAudioPanelLayoutType InPanelLayoutType);

		void StartProcessing();
		void StopProcessing();

		UAudioBus* GetAudioBus() const;
		TSharedRef<SWidget> GetPanelWidget() const;

	private:
		FAudioOscilloscopePanelStyle OscilloscopePanelStyle;

		TSharedPtr<FWaveformAudioSamplesDataProvider> AudioSamplesDataProvider = nullptr;
		TSharedPtr<SAudioOscilloscopePanelWidget> OscilloscopePanelWidget      = nullptr;

		TStrongObjectPtr<UAudioBus> AudioBus = nullptr;
	};
} // namespace AudioWidgets
