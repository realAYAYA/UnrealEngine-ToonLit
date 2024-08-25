// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMetersDashboardViewFactory.h"

#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsStyle.h"
#include "Providers/AudioBusProvider.h"
#include "Providers/SoundSubmixProvider.h"
#include "Sound/AudioBus.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "Views/AudioBusesDashboardViewFactory.h"
#include "Views/SubmixesDashboardViewFactory.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	FName FAudioMetersDashboardViewFactory::GetName() const
	{
		return "AudioMeters";
	}

	FText FAudioMetersDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_AudioMetersTab_DisplayName", "Audio Meters");
	}

	EDefaultDashboardTabStack FAudioMetersDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::AudioMeters;
	}

	FSlateIcon FAudioMetersDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Submix");
	}

	TSharedRef<SWidget> FAudioMetersDashboardViewFactory::MakeWidget()
	{
		if (!OnSubmixAssetCheckedHandle.IsValid())
		{
			OnSubmixAssetCheckedHandle = FSubmixesDashboardViewFactory::OnSubmixAssetChecked.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnSubmixAssetChecked);
		}

		if (!OnAudioBusAssetCheckedHandle.IsValid())
		{
			OnAudioBusAssetCheckedHandle = FAudioBusesDashboardViewFactory::OnAudioBusAssetChecked.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetChecked);
		}

		if (!OnSubmixAssetRemovedHandle.IsValid())
		{
			OnSubmixAssetRemovedHandle = FSoundSubmixProvider::OnSubmixAssetRemoved.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAssetRemoved);
		}

		if (!OnAudioBusAssetRemovedHandle.IsValid())
		{
			OnAudioBusAssetRemovedHandle = FAudioBusProvider::OnAudioBusAssetRemoved.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAssetRemoved);
		}
		
		if (!MeterViewsScrollBox.IsValid())
		{
			SAssignNew(MeterViewsScrollBox, SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(AudioMeterViewsContainer, SHorizontalBox)
			];
		}

		return MeterViewsScrollBox.ToSharedRef();
	}

	void FAudioMetersDashboardViewFactory::HandleOnSubmixAssetChecked(const bool bInIsChecked, const TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		if (!InSoundSubmix.IsValid())
		{
			return;
		}

		const uint32 SubmixUniqueId = InSoundSubmix->GetUniqueID();

		if (bInIsChecked)
		{
			AudioMeterViews.Add(SubmixUniqueId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<USoundSubmix>>(), InSoundSubmix)));

			AudioMeterViewsContainer->AddSlot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				AudioMeterViews[SubmixUniqueId]->GetWidget()
			];
		}
		else
		{
			if (AudioMeterViews.Contains(SubmixUniqueId))
			{
				AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[SubmixUniqueId]->GetWidget());
				AudioMeterViews.Remove(SubmixUniqueId);
			}
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetChecked(const bool bInIsChecked, const TWeakObjectPtr<UAudioBus> InAudioBus)
	{
		if (!InAudioBus.IsValid())
		{
			return;
		}

		const uint32 AudioBusUniqueId = InAudioBus->GetUniqueID();

		if (bInIsChecked)
		{
			AudioMeterViews.Add(AudioBusUniqueId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<UAudioBus>>(), InAudioBus)));

			AudioMeterViewsContainer->AddSlot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				AudioMeterViews[AudioBusUniqueId]->GetWidget()
			];
		}
		else
		{
			if (AudioMeterViews.Contains(AudioBusUniqueId))
			{
				AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[AudioBusUniqueId]->GetWidget());
				AudioMeterViews.Remove(AudioBusUniqueId);
			}
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnAssetRemoved(const TWeakObjectPtr<UObject> InAsset)
	{
		if (!InAsset.IsValid())
		{
			return;
		}

		if (const uint32 AssetUniqueId = InAsset->GetUniqueID();
			AudioMeterViews.Contains(AssetUniqueId))
		{
			AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[AssetUniqueId]->GetWidget());
			AudioMeterViews.Remove(AssetUniqueId);
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
