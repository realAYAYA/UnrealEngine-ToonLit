// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDefines.h"
#include "AudioVectorscopePanelStyle.h"
#include "AudioWidgetsEnums.h"
#include "Sound/AudioBus.h"
#include "UObject/StrongObjectPtr.h"
#include "WaveformAudioSamplesDataProvider.h"

class SAudioVectorscopePanelWidget;

namespace AudioWidgets
{
	class FWaveformAudioSamplesDataProvider;

	class AUDIOWIDGETS_API FAudioVectorscope
	{
	public:
		FAudioVectorscope(Audio::FDeviceId InAudioDeviceId,
			const uint32 InNumChannels, 
			const float InTimeWindowMs, 
			const float InMaxTimeWindowMs, 
			const float InAnalysisPeriodMs, 
			const EAudioPanelLayoutType InPanelLayoutType);

		void CreateAudioBus(const uint32 InNumChannels);
		void CreateDataProvider(Audio::FDeviceId InAudioDeviceId, const float InTimeWindowMs, const float InMaxTimeWindowMs, const float InAnalysisPeriodMs);
		void CreateVectorscopeWidget(const EAudioPanelLayoutType InPanelLayoutType);

		void StartProcessing();
		void StopProcessing();

		UAudioBus* GetAudioBus() const;
		TSharedRef<SWidget> GetPanelWidget() const;

	private:
		FAudioVectorscopePanelStyle VectorscopePanelStyle;

		TSharedPtr<FWaveformAudioSamplesDataProvider> AudioSamplesDataProvider = nullptr;
		TSharedPtr<SAudioVectorscopePanelWidget> VectorscopePanelWidget        = nullptr;

		TStrongObjectPtr<UAudioBus> AudioBus = nullptr;
	};
} // namespace AudioWidgets
