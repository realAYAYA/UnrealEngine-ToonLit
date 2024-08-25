// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/OutputOscilloscopeDashboardViewFactory.h"

#include "Analyzers/AudioOscilloscopeAnalyzer.h"
#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsStyle.h"
#include "AudioOscilloscope.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "SubmixesDashboardViewFactory.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Colors/SColorBlock.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	FName FOutputOscilloscopeDashboardViewFactory::GetName() const
	{
		return "OutputOscilloscope";
	}

	FText FOutputOscilloscopeDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_DashboardsOscilloscopeTab_DisplayName", "Output Oscilloscope");
	}

	EDefaultDashboardTabStack FOutputOscilloscopeDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Oscilloscope;
	}

	FSlateIcon FOutputOscilloscopeDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon");
	}

	TSharedRef<SWidget> FOutputOscilloscopeDashboardViewFactory::MakeWidget()
	{
		// Create OutputOscilloscopeAnalyzer (use MainSubmix as default)
		if (TWeakObjectPtr<UAudioSettings> AudioSettings = GetMutableDefault<UAudioSettings>();
			AudioSettings.IsValid())
		{
			MainSubmix = Cast<USoundSubmix>(AudioSettings->MasterSubmix.ResolveObject());

			if (MainSubmix)
			{
				OutputOscilloscopeAnalyzer = MakeShared<FAudioOscilloscopeAnalyzer>(MainSubmix);

				FDashboardFactory::OnActiveAudioDeviceChanged.AddSP(this, &FOutputOscilloscopeDashboardViewFactory::HandleOnActiveAudioDeviceChanged);
				FSubmixesDashboardViewFactory::OnSubmixSelectionChanged.AddSP(this, &FOutputOscilloscopeDashboardViewFactory::HandleOnSubmixSelectionChanged);
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
				OutputOscilloscopeAnalyzer.IsValid() ? OutputOscilloscopeAnalyzer->GetAudioOscilloscope()->GetPanelWidget() : SNullWidget::NullWidget
			]
		];
	}

	void FOutputOscilloscopeDashboardViewFactory::HandleOnActiveAudioDeviceChanged()
	{
		if (OutputOscilloscopeAnalyzer.IsValid())
		{
			OutputOscilloscopeAnalyzer->RebuildAudioOscilloscope(MainSubmix);
		}
	}

	void FOutputOscilloscopeDashboardViewFactory::HandleOnSubmixSelectionChanged(const TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		if (OutputOscilloscopeAnalyzer.IsValid())
		{
			OutputOscilloscopeAnalyzer->RebuildAudioOscilloscope(InSoundSubmix);
		}
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
