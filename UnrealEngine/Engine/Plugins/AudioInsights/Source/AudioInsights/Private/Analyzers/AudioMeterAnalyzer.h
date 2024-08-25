// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

namespace AudioWidgets { class FAudioMeter; }

class UAudioBus;

namespace UE::Audio::Insights
{
	class FAudioMeterAnalyzer : public TSharedFromThis<FAudioMeterAnalyzer>
	{
	public:
		explicit FAudioMeterAnalyzer(TWeakObjectPtr<UAudioBus> InExternalAudioBus = nullptr);
		virtual ~FAudioMeterAnalyzer() = default;

		TSharedRef<AudioWidgets::FAudioMeter> GetAudioMeter() { return AudioMeter; };

		void RebuildAudioMeter(TWeakObjectPtr<UAudioBus> InExternalAudioBus = nullptr);

	private:
		TSharedRef<AudioWidgets::FAudioMeter> AudioMeter;
	};
} // namespace UE::Audio::Insights
