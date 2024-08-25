// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMeterAnalyzer.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

namespace AudioWidgets { class FAudioMeter; }

class SAudioMeter;
class USoundSubmix;

namespace UE::Audio::Insights
{
	class FAudioMeterSubmixAnalyzer final : public TSharedFromThis<FAudioMeterSubmixAnalyzer>
	{
	public:
		FAudioMeterSubmixAnalyzer(TWeakObjectPtr<USoundSubmix> InSoundSubmix);
		~FAudioMeterSubmixAnalyzer();

		// Rebuilds the audio meter and registers the newly selected submix
		void SetSubmix(TWeakObjectPtr<USoundSubmix> InSoundSubmix);

		TSharedRef<SAudioMeter> GetWidget();
	private:
		void UnregisterAudioBusFromSubmix();

		TWeakObjectPtr<USoundSubmix> SoundSubmix;
		FAudioMeterAnalyzer AudioMeterAnalyzer;
	};
} // namespace UE::Audio::Insights
