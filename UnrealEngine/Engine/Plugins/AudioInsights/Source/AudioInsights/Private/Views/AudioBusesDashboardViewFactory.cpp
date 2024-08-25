// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/AudioBusesDashboardViewFactory.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "Providers/AudioBusProvider.h"
#include "Sound/AudioBus.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	namespace AudioBusesPrivate
	{
		const FAudioBusAssetDashboardEntry& CastEntry(const IDashboardDataViewEntry& InData)
		{
			return static_cast<const FAudioBusAssetDashboardEntry&>(InData);
		};
	}

	FAudioBusesDashboardViewFactory::FAudioBusesDashboardViewFactory()
	{
		FAudioBusProvider::OnAudioBusAssetAdded.AddRaw(this, &FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated);
		FAudioBusProvider::OnAudioBusAssetRemoved.AddRaw(this, &FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated);
		FAudioBusProvider::OnAudioBusAssetListUpdated.AddRaw(this, &FAudioBusesDashboardViewFactory::RequestListRefresh);

		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			MakeShared<FAudioBusProvider>()
		};

		SortByColumn = "Name";
		SortMode     = EColumnSortMode::Ascending;
	}

	FAudioBusesDashboardViewFactory::~FAudioBusesDashboardViewFactory()
	{
		FAudioBusProvider::OnAudioBusAssetAdded.RemoveAll(this);
		FAudioBusProvider::OnAudioBusAssetRemoved.RemoveAll(this);
		FAudioBusProvider::OnAudioBusAssetListUpdated.RemoveAll(this);
	}

	FName FAudioBusesDashboardViewFactory::GetName() const
	{
		return "AudioBuses";
	}

	FText FAudioBusesDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_AudioBuses_DisplayName", "Audio Buses");
	}

	TSharedRef<SWidget> FAudioBusesDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName)
	{
		if (InColumnName == "Name")
		{
			const TWeakObjectPtr<UAudioBus> AudioBus = AudioBusesPrivate::CastEntry(InRowData.Get()).AudioBus;

			AudioBusCheckboxCheckedStates.FindOrAdd(AudioBus, false);

			const FColumnData& ColumnData = GetColumns()[InColumnName];
			const FText Label = ColumnData.GetDisplayValue(InRowData.Get());

			return (Label.IsEmpty()) ? 
				SNullWidget::NullWidget :
				SNew(SHorizontalBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				+ SHorizontalBox::Slot()
				.FillWidth(0.1f)
				[
					SNew(SCheckBox)
					.IsChecked(AudioBusCheckboxCheckedStates.Contains(AudioBus) && AudioBusCheckboxCheckedStates[AudioBus] == true)
					.OnCheckStateChanged_Lambda([AudioBus, this](ECheckBoxState NewState)
					{
						if (AudioBusCheckboxCheckedStates.Contains(AudioBus))
						{
							AudioBusCheckboxCheckedStates[AudioBus] = NewState == ECheckBoxState::Checked;
						}

						OnAudioBusAssetChecked.Broadcast(NewState == ECheckBoxState::Checked, AudioBus);
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.8f)
				[
					SNew(STextBlock)
					.Text(Label)
					.MinDesiredWidth(300)
					.OnDoubleClicked_Lambda([this, InRowData](const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
					{
						if (GEditor)
						{
							if (TSharedPtr<IObjectDashboardEntry> ObjectData = StaticCastSharedPtr<IObjectDashboardEntry>(InRowData.ToSharedPtr()); 
								ObjectData.IsValid())
							{
								if (const UObject* Object = ObjectData->GetObject();
									Object && Object->IsAsset())
								{
									GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
									return FReply::Handled();
								}
							}
						}

						return FReply::Unhandled();
					})
				];
		}

		return SNullWidget::NullWidget;
	}

	void FAudioBusesDashboardViewFactory::ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason)
	{
		const FString FilterString = GetSearchFilterText().ToString();
		
		FTraceTableDashboardViewFactory::FilterEntries<FAudioBusProvider>([&FilterString](const IDashboardDataViewEntry& Entry)
		{
			const FAudioBusAssetDashboardEntry& AudioBusEntry = static_cast<const FAudioBusAssetDashboardEntry&>(Entry);
			
			return !AudioBusEntry.GetDisplayName().ToString().Contains(FilterString);
		});
	}

	FSlateIcon FAudioBusesDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon");
	}

	EDefaultDashboardTabStack FAudioBusesDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	TSharedRef<SWidget> FAudioBusesDashboardViewFactory::MakeWidget()
	{
		if (!DashboardWidget.IsValid())
		{
			DashboardWidget = FTraceTableDashboardViewFactory::MakeWidget();

			if (FilteredEntriesListView.IsValid())
			{
				FilteredEntriesListView->SetSelectionMode(ESelectionMode::Single);
			}
		}

		return DashboardWidget->AsShared();
	}

	const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& FAudioBusesDashboardViewFactory::GetColumns() const
	{
		auto CreateColumnData = []()
		{
			return TMap<FName, FTraceTableDashboardViewFactory::FColumnData>
			{
				{
					"Name",
					{
						LOCTEXT("AudioBuses_NameColumnDisplayName", "Name"),
						[](const IDashboardDataViewEntry& InData) { return AudioBusesPrivate::CastEntry(InData).GetDisplayName(); },
						false,		/* bDefaultHidden */
						0.88f,		/* FillWidth */
						EHorizontalAlignment::HAlign_Left
					}
				}
			};
		};
		
		static const TMap<FName, FTraceTableDashboardViewFactory::FColumnData> ColumnData = CreateColumnData();
		
		return ColumnData;
	}

	void FAudioBusesDashboardViewFactory::SortTable()
	{
		if (SortByColumn == "Name")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return AData.GetDisplayName().CompareToCaseIgnored(BData.GetDisplayName()) < 0;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return BData.GetDisplayName().CompareToCaseIgnored(AData.GetDisplayName()) < 0;
				});
			}
		}
	}

	void FAudioBusesDashboardViewFactory::RequestListRefresh()
	{
		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RequestListRefresh();
		}
	}

	void FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated(const TWeakObjectPtr<UObject> InAsset)
	{
		RequestListRefresh();
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
