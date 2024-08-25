// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/SubmixesDashboardViewFactory.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "Providers/SoundSubmixProvider.h"
#include "Sound/SoundSubmix.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	namespace SubmixesPrivate
	{
		const FSoundSubmixAssetDashboardEntry& CastEntry(const IDashboardDataViewEntry& InData)
		{
			return static_cast<const FSoundSubmixAssetDashboardEntry&>(InData);
		};
	}

	FSubmixesDashboardViewFactory::FSubmixesDashboardViewFactory()
	{
		FSoundSubmixProvider::OnSubmixAssetAdded.AddRaw(this, &FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated);
		FSoundSubmixProvider::OnSubmixAssetRemoved.AddRaw(this, &FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated);
		FSoundSubmixProvider::OnSubmixAssetListUpdated.AddRaw(this, &FSubmixesDashboardViewFactory::RequestListRefresh);

		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			MakeShared<FSoundSubmixProvider>()
		};

		SortByColumn = "Name";
		SortMode     = EColumnSortMode::Ascending;
	}

	FSubmixesDashboardViewFactory::~FSubmixesDashboardViewFactory()
	{
		FSoundSubmixProvider::OnSubmixAssetAdded.RemoveAll(this);
		FSoundSubmixProvider::OnSubmixAssetRemoved.RemoveAll(this);
		FSoundSubmixProvider::OnSubmixAssetListUpdated.RemoveAll(this);
	}

	FName FSubmixesDashboardViewFactory::GetName() const
	{
		return "Submixes";
	}

	FText FSubmixesDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_Submixes_DisplayName", "Submixes");
	}

	TSharedRef<SWidget> FSubmixesDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName)
	{
		if (InColumnName == "Name")
		{
			const TWeakObjectPtr<USoundSubmix> SoundSubmix = SubmixesPrivate::CastEntry(InRowData.Get()).SoundSubmix;

			SubmixCheckboxCheckedStates.FindOrAdd(SoundSubmix, false);

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
					.IsChecked(SubmixCheckboxCheckedStates.Contains(SoundSubmix) && SubmixCheckboxCheckedStates[SoundSubmix] == true)
					.OnCheckStateChanged_Lambda([SoundSubmix, this](ECheckBoxState NewState)
					{
						if (SubmixCheckboxCheckedStates.Contains(SoundSubmix))
						{
							SubmixCheckboxCheckedStates[SoundSubmix] = NewState == ECheckBoxState::Checked;
						}

						OnSubmixAssetChecked.Broadcast(NewState == ECheckBoxState::Checked, SoundSubmix);
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

	void FSubmixesDashboardViewFactory::ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason)
	{
		const FString FilterString = GetSearchFilterText().ToString();
		
		FTraceTableDashboardViewFactory::FilterEntries<FSoundSubmixProvider>([&FilterString](const IDashboardDataViewEntry& Entry)
		{
			const FSoundSubmixAssetDashboardEntry& SubmixEntry = static_cast<const FSoundSubmixAssetDashboardEntry&>(Entry);
			
			return !SubmixEntry.GetDisplayName().ToString().Contains(FilterString);
		});
	}

	FSlateIcon FSubmixesDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Submix");
	}

	EDefaultDashboardTabStack FSubmixesDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	TSharedRef<SWidget> FSubmixesDashboardViewFactory::MakeWidget()
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

	const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& FSubmixesDashboardViewFactory::GetColumns() const
	{
		auto CreateColumnData = []()
		{
			return TMap<FName, FTraceTableDashboardViewFactory::FColumnData>
			{
				{
					"Name",
					{
						LOCTEXT("Submixes_NameColumnDisplayName", "Name"),
						[](const IDashboardDataViewEntry& InData) { return SubmixesPrivate::CastEntry(InData).GetDisplayName(); },
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

	void FSubmixesDashboardViewFactory::SortTable()
	{
		if (SortByColumn == "Name")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

					return AData.GetDisplayName().CompareToCaseIgnored(BData.GetDisplayName()) < 0;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

					return BData.GetDisplayName().CompareToCaseIgnored(AData.GetDisplayName()) < 0;
				});
			}
		}
	}

	void FSubmixesDashboardViewFactory::OnSelectionChanged(TSharedPtr<IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (SelectedItem.IsValid())
		{
			const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(*SelectedItem.Get());

			OnSubmixSelectionChanged.Broadcast(SoundSubmixAssetDashboardEntry.SoundSubmix);
		}
	}

	void FSubmixesDashboardViewFactory::RequestListRefresh()
	{
		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RequestListRefresh();
		}
	}

	void FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated(const TWeakObjectPtr<UObject> InAsset)
	{
		RequestListRefresh();
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
