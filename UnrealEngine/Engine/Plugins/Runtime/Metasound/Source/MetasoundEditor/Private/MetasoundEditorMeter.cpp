// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorMeter.h"
#include "Editor.h"

namespace Metasound
{
	namespace Editor
	{
		FEditorMeter::FEditorMeter()
			: Widget(SNew(SAudioMeter)
				.Orientation(EOrientation::Orient_Vertical)
				.BackgroundColor(FLinearColor::Transparent)

				// TODO: Move to editor style
				.MeterBackgroundColor(FLinearColor(0.031f, 0.031f, 0.031f, 1.0f))
				.MeterValueColor(FLinearColor(0.025719f, 0.208333f, 0.069907f, 1.0f))
				.MeterPeakColor(FLinearColor(0.24349f, 0.708333f, 0.357002f, 1.0f))
				.MeterClippingColor(FLinearColor(1.0f, 0.0f, 0.112334f, 1.0f))
				.MeterScaleColor(FLinearColor(0.017642f, 0.017642f, 0.017642f, 1.0f))
				.MeterScaleLabelColor(FLinearColor(0.442708f, 0.442708f, 0.442708f, 1.0f))
			)
		{
		}

		UAudioBus* FEditorMeter::GetAudioBus() const
		{
			return AudioBus.Get();
		}

		TSharedPtr<SAudioMeter> FEditorMeter::GetWidget() const
		{
			return Widget;
		}

		void FEditorMeter::Init(int32 InNumChannels)
		{
			check(InNumChannels > 0);

			Settings = TStrongObjectPtr(NewObject<UMeterSettings>());
			Settings->PeakHoldTime = 4000.0f;

			Analyzer = TStrongObjectPtr(NewObject<UMeterAnalyzer>());
			Analyzer->Settings = Settings.Get();

			AudioBus = TStrongObjectPtr(NewObject<UAudioBus>());
			AudioBus->AudioBusChannels = EAudioBusChannels(InNumChannels - 1);

			ResultsDelegateHandle = Analyzer->OnLatestPerChannelMeterResultsNative.AddRaw(this, &FEditorMeter::OnMeterOutput);

			if (ensure(GEditor))
			{
				UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
				if (ensure(EditorWorld))
				{
					Analyzer->StartAnalyzing(EditorWorld, AudioBus.Get());
				}
			}

			constexpr float DefaultMeterValue = -60.0f;
			constexpr float DefaultPeakValue = -60.0f;
			ChannelInfo.Init(FMeterChannelInfo{ DefaultMeterValue, DefaultPeakValue }, InNumChannels);

			if (Widget.IsValid())
			{
				Widget->SetMeterChannelInfo(ChannelInfo);
			}
		}

		void FEditorMeter::OnMeterOutput(UMeterAnalyzer* InMeterAnalyzer, int32 ChannelIndex, const FMeterResults& InMeterResults)
		{
			if (InMeterAnalyzer == Analyzer.Get())
			{
				FMeterChannelInfo NewChannelInfo;
				NewChannelInfo.MeterValue = InMeterResults.MeterValue;
				NewChannelInfo.PeakValue = InMeterResults.PeakValue;

				if (ChannelIndex < ChannelInfo.Num())
				{
					ChannelInfo[ChannelIndex] = MoveTemp(NewChannelInfo);
				}

				// Update the widget if this is the last channel
				if (ChannelIndex == ChannelInfo.Num() - 1)
				{
					if (Widget.IsValid())
					{
						Widget->SetMeterChannelInfo(ChannelInfo);
					}
				}
			}
		}

		void FEditorMeter::Teardown()
		{
			if (Analyzer.IsValid() && ResultsDelegateHandle.IsValid())
			{
				if (ensure(GEditor))
				{
					UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
					if (ensure(EditorWorld))
					{
						Analyzer->StopAnalyzing(EditorWorld);
					}
				}
				Analyzer->OnLatestPerChannelMeterResultsNative.Remove(ResultsDelegateHandle);
				Analyzer.Reset();
				ResultsDelegateHandle.Reset();
			}

			AudioBus.Reset();
			ChannelInfo.Reset();
			Settings.Reset();
		}
	} // namespace Editor
} // namespace Metasound
