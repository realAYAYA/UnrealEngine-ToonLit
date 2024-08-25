// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMeterView.h"

#include "Analyzers/AudioMeterSubmixAnalyzer.h"
#include "AudioInsightsDashboardFactory.h"
#include "AudioMeter.h"
#include "Sound/AudioBus.h"
#include "Sound/SoundSubmix.h"
#include "Views/SubmixesDashboardViewFactory.h"
#include "Widgets/SNullWidget.h"

namespace UE::Audio::Insights
{
	FAudioMeterView::FAudioMeterView(FAudioAssetVariant InAudioAssetVariant)
		: AudioAssetVariant(InAudioAssetVariant)
		, AudioMeterAnalyzerVariant(MakeAudioMeterAnalyzerVariant(InAudioAssetVariant))
		, AudioAssetNameTextBlock(MakeAudioAssetNameTextBlock(InAudioAssetVariant))
		, AudioMeterViewWidget(MakeWidget())
		, OnActiveAudioDeviceChangedHandle(FDashboardFactory::OnActiveAudioDeviceChanged.AddRaw(this, &FAudioMeterView::HandleOnActiveAudioDeviceChanged))
	{
		
	}

	FAudioMeterView::~FAudioMeterView()
	{
		FDashboardFactory::OnActiveAudioDeviceChanged.Remove(OnActiveAudioDeviceChangedHandle);
	}

	FAudioMeterView::FAudioMeterVariant FAudioMeterView::MakeAudioMeterAnalyzerVariant(const FAudioAssetVariant InAudioAssetVariant)
	{
		switch (AudioAssetVariant.GetIndex())
		{
			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<UAudioBus>>():
			{
				return FAudioMeterVariant(TInPlaceType<TSharedPtr<FAudioMeterAnalyzer>>(), MakeShared<FAudioMeterAnalyzer>(InAudioAssetVariant.Get<TWeakObjectPtr<UAudioBus>>()));
			}

			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<USoundSubmix>>():
			{
				return FAudioMeterVariant(TInPlaceType<TSharedPtr<FAudioMeterSubmixAnalyzer>>(), MakeShared<FAudioMeterSubmixAnalyzer>(InAudioAssetVariant.Get<TWeakObjectPtr<USoundSubmix>>()));
			}

			default:
				return FAudioMeterView::FAudioMeterVariant();
		}
	}

	TSharedRef<STextBlock> FAudioMeterView::MakeAudioAssetNameTextBlock(const FAudioAssetVariant InAudioAssetVariant)
	{
		switch (InAudioAssetVariant.GetIndex())
		{
			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<UAudioBus>>():
			{
				if (TWeakObjectPtr<UAudioBus> AudioBusAsset = InAudioAssetVariant.Get<TWeakObjectPtr<UAudioBus>>();
					AudioBusAsset.IsValid())
				{
					return SNew(STextBlock)
						.Text_Lambda([AudioBusAsset] { return AudioBusAsset.IsValid() ? FText::FromString(AudioBusAsset->GetName()) : FText::GetEmpty(); })
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(FColor(80, 200, 255)));
				}
			}
			break;

			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<USoundSubmix>>():
			{
				if (TWeakObjectPtr<USoundSubmix> SoundSubmixAsset = AudioAssetVariant.Get<TWeakObjectPtr<USoundSubmix>>();
					SoundSubmixAsset.IsValid())
				{
					return SNew(STextBlock)
						.Text_Lambda([SoundSubmixAsset] { return SoundSubmixAsset.IsValid() ? FText::FromString(SoundSubmixAsset->GetName()) : FText::GetEmpty(); })
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(FColor(30, 240, 90)));
				}
			}
			break;
		}

		return SNew(STextBlock);
	}

	TSharedRef<SWidget> FAudioMeterView::GetAudioMeterAnalyzerWidget() const
	{
		switch (AudioMeterAnalyzerVariant.GetIndex())
		{
			case FAudioMeterVariant::IndexOfType<TSharedPtr<FAudioMeterAnalyzer>>():
			{
				TSharedPtr<FAudioMeterAnalyzer> AudioMeterAnalyzer = AudioMeterAnalyzerVariant.Get<TSharedPtr<FAudioMeterAnalyzer>>();
				return AudioMeterAnalyzer.IsValid() ? AudioMeterAnalyzer->GetAudioMeter()->GetWidget() : SNullWidget::NullWidget;
			}

			case FAudioMeterVariant::IndexOfType<TSharedPtr<FAudioMeterSubmixAnalyzer>>():
			{
				TSharedPtr<FAudioMeterSubmixAnalyzer> AudioMeterSubmixAnalyzer = AudioMeterAnalyzerVariant.Get<TSharedPtr<FAudioMeterSubmixAnalyzer>>();
				return AudioMeterSubmixAnalyzer.IsValid() ? AudioMeterSubmixAnalyzer->GetWidget() : SNullWidget::NullWidget;
			}

			default:
				return SNullWidget::NullWidget;
		}
	}

	TSharedRef<SWidget> FAudioMeterView::MakeWidget()
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SVerticalBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				// Height padding
				+ SVerticalBox::Slot()
				.FillHeight(0.05)
				[
					SNew(SBox)
				]
				// Audio Meter container
				+ SVerticalBox::Slot()
				.FillHeight(0.8)
				[
					// Audio Meter
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						GetAudioMeterAnalyzerWidget()
					]
				]
				// Height padding
				+ SVerticalBox::Slot()
				.FillHeight(0.025)
				[
					SNew(SBox)
				]
				// Audio aaset name label
				+ SVerticalBox::Slot()
				.FillHeight(0.1)
				[
					AudioAssetNameTextBlock
				]
				// Height padding
				+ SVerticalBox::Slot()
				.FillHeight(0.025)
				[
					SNew(SBox)
				]
			];
	}

	void FAudioMeterView::HandleOnActiveAudioDeviceChanged()
	{
		switch (AudioAssetVariant.GetIndex())
		{
			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<UAudioBus>>():
			{
				if (TSharedPtr<FAudioMeterAnalyzer> AudioMeterAnalyzer = AudioMeterAnalyzerVariant.Get<TSharedPtr<FAudioMeterAnalyzer>>();
					AudioMeterAnalyzer.IsValid())
				{
					AudioMeterAnalyzer->RebuildAudioMeter(AudioAssetVariant.Get<TWeakObjectPtr<UAudioBus>>());
				}
			}
			break;

			case FAudioAssetVariant::IndexOfType<TWeakObjectPtr<USoundSubmix>>():
			{
				if (TSharedPtr<FAudioMeterSubmixAnalyzer> AudioMeterSubmixAnalyzer = AudioMeterAnalyzerVariant.Get<TSharedPtr<FAudioMeterSubmixAnalyzer>>();
					AudioMeterSubmixAnalyzer.IsValid())
				{
					AudioMeterSubmixAnalyzer->SetSubmix(AudioAssetVariant.Get<TWeakObjectPtr<USoundSubmix>>());
				}
			}
			break;

			default:
				break;
		}
	}
} // namespace UE::Audio::Insights
