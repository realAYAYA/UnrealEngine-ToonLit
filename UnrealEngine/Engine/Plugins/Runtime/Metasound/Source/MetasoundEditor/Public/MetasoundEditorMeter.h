// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Meter.h"
#include "SAudioMeter.h"
#include "Sound/AudioBus.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"


namespace Metasound
{
	namespace Editor
	{
		class FEditorMeter
		{
		public:
			FEditorMeter();

			UAudioBus* GetAudioBus() const;

			TSharedPtr<SAudioMeter> GetWidget() const;

			void Teardown();

			void Init(int32 InNumChannels);

		protected:
			void OnMeterOutput(UMeterAnalyzer* InMeterAnalyzer, int32 ChannelIndex, const FMeterResults& InMeterResults);

		private:
			/** Metasound analyzer object. */
			TStrongObjectPtr<UMeterAnalyzer> Analyzer;

			/** The audio bus used for analysis. */
			TStrongObjectPtr<UAudioBus> AudioBus;

			/** Cached channel info for the meter. */
			TArray<FMeterChannelInfo> ChannelInfo;

			/** Handle for results delegate for MetaSound meter analyzer. */
			FDelegateHandle ResultsDelegateHandle;

			/** Meter settings. */
			TStrongObjectPtr<UMeterSettings> Settings;

			/** MetaSound Output Meter widget */
			TSharedPtr<SAudioMeter> Widget;
		};
	} // namespace Editor
} // namespace Metasound