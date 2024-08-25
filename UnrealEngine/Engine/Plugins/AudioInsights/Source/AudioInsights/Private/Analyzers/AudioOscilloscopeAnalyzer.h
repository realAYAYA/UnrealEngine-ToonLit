// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioWidgetsEnums.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

namespace AudioWidgets { class FAudioOscilloscope; }

class USoundSubmix;

namespace UE::Audio::Insights
{
	class FAudioOscilloscopeAnalyzer : public TSharedFromThis<FAudioOscilloscopeAnalyzer>
	{
	public:
		FAudioOscilloscopeAnalyzer(TWeakObjectPtr<USoundSubmix> InSoundSubmix);
		virtual ~FAudioOscilloscopeAnalyzer();

		TSharedRef<AudioWidgets::FAudioOscilloscope> GetAudioOscilloscope() { return AudioOscilloscope; };

		void RebuildAudioOscilloscope(TWeakObjectPtr<USoundSubmix> InSoundSubmix);

	private:
		void CleanupAudioOscilloscope();

		TSharedRef<AudioWidgets::FAudioOscilloscope> AudioOscilloscope;
		TWeakObjectPtr<USoundSubmix> SoundSubmix;
		
		static constexpr float TimeWindowMs     = 10.0f;
		static constexpr float MaxTimeWindowMs  = 10.0f;
		static constexpr float AnalysisPeriodMs = 10.0f;
		static constexpr EAudioPanelLayoutType PanelLayoutType = EAudioPanelLayoutType::Basic;
	};
} // namespace UE::Audio::Insights
