// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioSpectrumAnalyzer.h"

#include "DSP/EnvelopeFollower.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FAudioSpectrumAnalyzer"

namespace AudioWidgets
{
	FAudioSpectrumAnalyzer::FAudioSpectrumAnalyzer(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus)
		: Widget(SNew(SAudioSpectrumPlot)
			.Clipping(EWidgetClipping::ClipToBounds)
			.DisplayFrequencyAxisLabels(false)
			.DisplaySoundLevelAxisLabels(false)
			.FrequencyAxisPixelBucketMode_Lambda([]() { return EAudioSpectrumPlotFrequencyAxisPixelBucketMode::Average; }) // Binding this property has the effect of hiding its context menu entry (it isn't much use for the ConstantQ analyzer).
			.OnGetAudioSpectrumData_Raw(this, &FAudioSpectrumAnalyzer::GetAudioSpectrumData))
	{
		ContextMenuExtension = Widget->AddContextMenuExtension(EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateRaw(this, &FAudioSpectrumAnalyzer::ExtendSpectrumPlotContextMenu));

		Init(InNumChannels, InAudioDeviceId, InExternalAudioBus);
	}

	FAudioSpectrumAnalyzer::~FAudioSpectrumAnalyzer()
	{
		Teardown();

		Widget->UnbindOnGetAudioSpectrumData();

		if (ContextMenuExtension.IsValid())
		{
			Widget->RemoveContextMenuExtension(ContextMenuExtension.ToSharedRef());
		}
	}

	UAudioBus* FAudioSpectrumAnalyzer::GetAudioBus() const
	{
		return AudioBus.Get();
	}

	TSharedRef<SWidget> FAudioSpectrumAnalyzer::GetWidget() const
	{
		return Widget->AsShared();
	}

	void FAudioSpectrumAnalyzer::Init(int32 InNumChannels, Audio::FDeviceId InAudioDeviceId, TObjectPtr<UAudioBus> InExternalAudioBus)
	{
		check(InNumChannels > 0);

		Teardown();

		Settings = TStrongObjectPtr(NewObject<UConstantQSettings>());
		Settings->SpectrumType = EAudioSpectrumType::PowerSpectrum;
		Settings->NumBandsPerOctave = 6.0f;
		Settings->NumBands = 61;
		Settings->StartingFrequencyHz = 20000.0f * FMath::Pow(0.5f, (Settings->NumBands - 1) / Settings->NumBandsPerOctave);
		Settings->FFTSize = EConstantQFFTSizeEnum::XXLarge;
		Settings->bDownmixToMono = true;
		Settings->BandWidthStretch = 2.0f;

		Analyzer = TStrongObjectPtr(NewObject<UConstantQAnalyzer>());
		Analyzer->Settings = Settings.Get();

		bUseExternalAudioBus = InExternalAudioBus != nullptr;

		AudioBus = bUseExternalAudioBus ? TStrongObjectPtr(InExternalAudioBus.Get()) : TStrongObjectPtr(NewObject<UAudioBus>());
		AudioBus->AudioBusChannels = EAudioBusChannels(InNumChannels - 1);

		ResultsDelegateHandle = Analyzer->OnConstantQResultsNative.AddRaw(this, &FAudioSpectrumAnalyzer::OnConstantQResults);

		Analyzer->StartAnalyzing(InAudioDeviceId, AudioBus.Get());
	}

	void FAudioSpectrumAnalyzer::OnConstantQResults(UConstantQAnalyzer* InSpectrumAnalyzer, int32 ChannelIndex, const TArray<FConstantQResults>& InSpectrumResultsArray)
	{
		if (InSpectrumAnalyzer == Analyzer.Get())
		{
			for (const FConstantQResults& SpectrumResults : InSpectrumResultsArray)
			{
				if (PrevTimeStamp.IsSet() && SpectrumResults.TimeSeconds > PrevTimeStamp.GetValue())
				{
					// Calculate AR smoother coefficients:
					const float DeltaT = (SpectrumResults.TimeSeconds - PrevTimeStamp.GetValue());
					Audio::FAttackRelease AttackRelease(1.0f / DeltaT, AttackTimeMsec, ReleaseTimeMsec, bIsAnalogAttackRelease);

					// Apply AR smoothing for each frequency:
					check(SpectrumResults.SpectrumValues.Num() == ARSmoothedSquaredMagnitudes.Num());
					for (int Index = 0; Index < SpectrumResults.SpectrumValues.Num(); Index++)
					{
						const float OldValue = ARSmoothedSquaredMagnitudes[Index];
						const float NewValue = SpectrumResults.SpectrumValues[Index];
						const float ARSmootherCoefficient = (NewValue >= OldValue) ? AttackRelease.GetAttackTimeSamples() : AttackRelease.GetReleaseTimeSamples();
						const float SmoothedValue = FMath::Lerp(NewValue, OldValue, ARSmootherCoefficient);
						ARSmoothedSquaredMagnitudes[Index] = Audio::UnderflowClamp(SmoothedValue);
					}
				}
				else
				{
					// Init center frequencies:
					CenterFrequencies.SetNumUninitialized(Analyzer->GetNumCenterFrequencies());
					Analyzer->GetCenterFrequencies(CenterFrequencies);

					// Init spectrum data:
					ARSmoothedSquaredMagnitudes = SpectrumResults.SpectrumValues;
				}

				PrevTimeStamp = SpectrumResults.TimeSeconds;
			}
		}
	}

	void FAudioSpectrumAnalyzer::Teardown()
	{
		if (Analyzer.IsValid() && Analyzer->IsValidLowLevel())
		{
			Analyzer->StopAnalyzing();
			if (ResultsDelegateHandle.IsValid())
			{
				Analyzer->OnConstantQResultsNative.Remove(ResultsDelegateHandle);
			}
			
			Analyzer.Reset();
		}

		ResultsDelegateHandle.Reset();
		PrevTimeStamp.Reset();
		CenterFrequencies.Empty();
		ARSmoothedSquaredMagnitudes.Empty();

		AudioBus.Reset();
		Settings.Reset();

		bUseExternalAudioBus = false;
	}

	FAudioPowerSpectrumData FAudioSpectrumAnalyzer::GetAudioSpectrumData() const
	{
		check(CenterFrequencies.Num() == ARSmoothedSquaredMagnitudes.Num());
		return FAudioPowerSpectrumData{ CenterFrequencies, ARSmoothedSquaredMagnitudes };
	}

	void FAudioSpectrumAnalyzer::ExtendSpectrumPlotContextMenu(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("AnalyzerSettings", LOCTEXT("AnalyzerSettings", "Analyzer Settings"));
		MenuBuilder.AddSubMenu(
			LOCTEXT("Ballistics", "Ballistics"),
			FText(),
			FNewMenuDelegate::CreateSP(this, &FAudioSpectrumAnalyzer::BuildBallisticsSubMenu));
		MenuBuilder.EndSection();
	}

	void FAudioSpectrumAnalyzer::BuildBallisticsSubMenu(FMenuBuilder& SubMenu)
	{
		SubMenu.AddMenuEntry(
			LOCTEXT("Analog", "Analog"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this]() { bIsAnalogAttackRelease = true; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]() { return bIsAnalogAttackRelease; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
		SubMenu.AddMenuEntry(
			LOCTEXT("Digital", "Digital"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this]() { bIsAnalogAttackRelease = false; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]() { return !bIsAnalogAttackRelease; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}

} // namespace AudioWidgets

#undef LOCTEXT_NAMESPACE
