// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ConstantQ.h"
#include "SAudioSpectrumPlot.h"
#include "Sound/AudioBus.h"
#include "UObject/StrongObjectPtr.h"

class UWorld;

namespace AudioWidgets
{
	/**
	 * Owns an analyzer and a corresponding Slate widget for displaying the resulting spectrum.
	 * Exponential time-smoothing is applied to the spectrum.
	 * Can either create an Audio Bus to analyze, or analyze the given Bus.
	 */
	class AUDIOWIDGETS_API FAudioSpectrumAnalyzer : public TSharedFromThis<FAudioSpectrumAnalyzer>
	{
	public:
		FAudioSpectrumAnalyzer(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus = nullptr);
		~FAudioSpectrumAnalyzer();

		UAudioBus* GetAudioBus() const;

		TSharedRef<SWidget> GetWidget() const;

		void Init(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus = nullptr);

	protected:
		void OnConstantQResults(UConstantQAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FConstantQResults>& InSpectrumResultsArray);
		FAudioPowerSpectrumData GetAudioSpectrumData() const;
		void ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder);
		void BuildBallisticsSubMenu(FMenuBuilder& SubMenu);

	private:
		void Teardown();

		/** Audio analyzer object. */
		TStrongObjectPtr<UConstantQAnalyzer> Analyzer;

		/** The audio bus used for analysis. */
		TStrongObjectPtr<UAudioBus> AudioBus;

		/** Meaning of spectrum data. */
		TArray<float> CenterFrequencies;

		/** Cached spectrum data, with AR smoothing applied. */
		TArray<float> ARSmoothedSquaredMagnitudes;

		/** Handle for results delegate for analyzer. */
		FDelegateHandle ResultsDelegateHandle;

		/** Analyzer settings. */
		TStrongObjectPtr<UConstantQSettings> Settings;

		/** Slate widget for spectrum display */
		TSharedPtr<SAudioSpectrumPlot> Widget;
		TSharedPtr<const FExtensionBase> ContextMenuExtension;

		bool bUseExternalAudioBus = false;

		TOptional<float> PrevTimeStamp;
		float AttackTimeMsec = 300.0f;
		float ReleaseTimeMsec = 300.0f;
		bool bIsAnalogAttackRelease = false;
	};
} // namespace AudioWidgets
