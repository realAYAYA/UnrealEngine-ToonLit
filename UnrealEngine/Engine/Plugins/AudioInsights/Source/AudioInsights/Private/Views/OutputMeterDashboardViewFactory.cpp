// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/OutputMeterDashboardViewFactory.h"

#include "Analyzers/AudioMeterSubmixAnalyzer.h"
#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsStyle.h"
#include "AudioMeter.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "SubmixesDashboardViewFactory.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	FName FOutputMeterDashboardViewFactory::GetName() const
	{
		return "OutputMeter";
	}

	FText FOutputMeterDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_DashboardsAudioMeterTab_DisplayName", "Output Meter");
	}

	EDefaultDashboardTabStack FOutputMeterDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::AudioMeter;
	}

	FSlateIcon FOutputMeterDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon");
	}

	TSharedRef<SWidget> FOutputMeterDashboardViewFactory::MakeWidget()
	{
		// Create OutputMeterSubmixAnalyzer (use MainSubmix as default)
		if (TWeakObjectPtr<UAudioSettings> AudioSettings = GetMutableDefault<UAudioSettings>();
			AudioSettings.IsValid())
		{
			MainSubmix = Cast<USoundSubmix>(AudioSettings->MasterSubmix.ResolveObject());

			if (MainSubmix)
			{
				OutputMeterSubmixAnalyzer = MakeShared<FAudioMeterSubmixAnalyzer>(MainSubmix);

				FDashboardFactory::OnActiveAudioDeviceChanged.AddSP(this, &FOutputMeterDashboardViewFactory::HandleOnActiveAudioDeviceChanged);
				FSubmixesDashboardViewFactory::OnSubmixSelectionChanged.AddSP(this, &FOutputMeterDashboardViewFactory::HandleOnSubmixSelectionChanged);
			}
		}

		return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SColorBlock)
			.Color(FSlateStyle::Get().GetColor("AudioInsights.Analyzers.BackgroundColor"))
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			[
				OutputMeterSubmixAnalyzer.IsValid() ? OutputMeterSubmixAnalyzer->GetWidget() : SNullWidget::NullWidget
			]
		];
	}

	void FOutputMeterDashboardViewFactory::HandleOnActiveAudioDeviceChanged()
	{
		if (OutputMeterSubmixAnalyzer.IsValid())
		{
			OutputMeterSubmixAnalyzer->SetSubmix(MainSubmix);
		}
	}

	void FOutputMeterDashboardViewFactory::HandleOnSubmixSelectionChanged(const TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		if (OutputMeterSubmixAnalyzer.IsValid())
		{
			OutputMeterSubmixAnalyzer->SetSubmix(InSoundSubmix);
		}
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
