// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/TableDashboardViewFactory.h"

#include "Algo/Transform.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsDashboardAssetCommands.h"
#include "AudioInsightsTraceProviderBase.h"
#include "Containers/Array.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Framework/Commands/UICommandList.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	void FTraceTableDashboardViewFactory::SRowWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<IDashboardDataViewEntry> InData, TSharedRef<FTraceTableDashboardViewFactory> InFactory)
	{
		Data = InData;
		Factory = InFactory;
		SMultiColumnTableRow<TSharedPtr<IDashboardDataViewEntry>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
	}

	TSharedRef<SWidget> FTraceTableDashboardViewFactory::SRowWidget::GenerateWidgetForColumn(const FName& Column)
	{
		return Factory->GenerateWidgetForColumn(Data->AsShared(), Column);
	}

	TSharedRef<SWidget> FTraceTableDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& Column)
	{
		const IDashboardDataViewEntry& RowData = InRowData.Get();
		const FColumnData& ColumnData = GetColumns()[Column];
		const FText Label = ColumnData.GetDisplayValue(RowData);
		if (Label.IsEmpty())
		{
			return SNullWidget::NullWidget;
		}

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(2)
		[
			SNew(STextBlock).Text(Label)
		];
	}

	FReply FTraceTableDashboardViewFactory::OnDataRowKeyInput(const FGeometry& Geometry, const FKeyEvent& KeyEvent) const
	{
		return FReply::Unhandled();
	}

	EColumnSortMode::Type FTraceTableDashboardViewFactory::GetColumnSortMode(const FName InColumnId) const
	{
		return (SortByColumn == InColumnId) ? SortMode : EColumnSortMode::None;
	}

	void FTraceTableDashboardViewFactory::RequestSort()
	{
		SortTable();

		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RequestListRefresh();
		}
	}

	void FTraceTableDashboardViewFactory::OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode)
	{
		SortByColumn = InColumnId;
		SortMode = InSortMode;
		RequestSort();
	}

	TSharedPtr<SWidget> FTraceTableDashboardViewFactory::OnConstructContextMenu()
	{
		return SNullWidget::NullWidget;
	}

	void FTraceTableDashboardViewFactory::OnSelectionChanged(TSharedPtr<IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo)
	{
		// To be optionally implemented by derived classes
	}

	FSlateColor FTraceTableDashboardViewFactory::GetRowColor(const TSharedPtr<IDashboardDataViewEntry>& InRowDataPtr)
	{
		return FSlateColor(FColor(255, 255, 255));
	}

	TSharedRef<SHeaderRow> FTraceTableDashboardViewFactory::MakeHeaderRowWidget()
	{
		TArray<FName> DefaultHiddenColumns;
		Algo::TransformIf(GetColumns(), DefaultHiddenColumns,
			[](const TPair<FName, FColumnData>& ColumnInfo) { return ColumnInfo.Value.bDefaultHidden; },
			[](const TPair<FName, FColumnData>& ColumnInfo) { return ColumnInfo.Key; });

		TSharedRef<SHeaderRow> HeaderRowWidget = SNew(SHeaderRow)
		.CanSelectGeneratedColumn(true); // Allows for showing/hiding columns

		// This only works if header row columns are added with slots and not programmatically
		// check in SHeaderRow::Construct: for ( FColumn* const Column : InArgs.Slots ) for more info
		// A potential alternative would be to delegate to the derived classes the SHeaderRow creation with slots
		//.HiddenColumnsList(DefaultHiddenColumns);

		for (const auto& [ColumnName, ColumnData] : GetColumns())
		{
			const SHeaderRow::FColumn::FArguments ColumnArgs = SHeaderRow::Column(ColumnName)
				.DefaultLabel(ColumnData.DisplayName)
				.HAlignCell(ColumnData.Alignment)
				.FillWidth(ColumnData.FillWidth)
				.SortMode(this, &FTraceTableDashboardViewFactory::GetColumnSortMode, ColumnName)
				.OnSort(this, &FTraceTableDashboardViewFactory::OnColumnSortModeChanged);

			// .HiddenColumnsList workaround:
			// simulate what SHeaderRow::AddColumn( const FColumn::FArguments& NewColumnArgs ) does but allowing us to modify the bIsVisible property
			// Memory handling (delete) is done by TIndirectArray<FColumn> Columns; defined in SHeaderRow
			SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(ColumnArgs);
			NewColumn->bIsVisible = !DefaultHiddenColumns.Contains(ColumnName);
			HeaderRowWidget->AddColumn(*NewColumn);
		}

		return HeaderRowWidget;
	}

	TSharedRef<SWidget> FTraceTableDashboardViewFactory::MakeWidget()
	{
		if (!DashboardWidget.IsValid())
		{
			FilteredEntriesListView = SNew(SListView<TSharedPtr<IDashboardDataViewEntry>>)
			.ListItemsSource(&DataViewEntries)
			.OnContextMenuOpening(this, &FTraceTableDashboardViewFactory::OnConstructContextMenu)
			.OnSelectionChanged(this, &FTraceTableDashboardViewFactory::OnSelectionChanged)
			.OnGenerateRow_Lambda([this](TSharedPtr<IDashboardDataViewEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return SNew(SRowWidget, OwnerTable, Item, AsShared());
			})
			.HeaderRow
			(
				MakeHeaderRowWidget()
			)
			.SelectionMode(ESelectionMode::Multi)
			.OnKeyDownHandler_Lambda([this](const FGeometry& Geometry, const FKeyEvent& KeyEvent)
			{
				return OnDataRowKeyInput(Geometry, KeyEvent);
			});

			DashboardWidget = SNew(SVerticalBox)
			.Clipping(EWidgetClipping::ClipToBounds)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SearchBoxWidget, SSearchBox)
					.SelectAllTextWhenFocused(true)
					.HintText(LOCTEXT("TableDashboardView_SearchBoxHintText", "Filter"))
					.MinDesiredWidth(100)
					.OnTextChanged(this, &FTraceTableDashboardViewFactory::SetSearchBoxFilterText)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0, 2)
			[
				FilteredEntriesListView->AsShared()
			];
		}

		return DashboardWidget->AsShared();
	}

	void FTraceTableDashboardViewFactory::SetSearchBoxFilterText(const FText& NewText)
	{
		SearchBoxFilterText = NewText;
		UpdateFilterReason = EProcessReason::FilterUpdated;
	}


	void FTraceTableDashboardViewFactory::RebuildFilteredEntriesListView()
	{
		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RebuildList();
		}
	}

	const FText& FTraceTableDashboardViewFactory::GetSearchFilterText() const
	{
		return SearchBoxFilterText;
	}

	void FTraceTableDashboardViewFactory::Tick(float InElapsed)
	{
		for (TSharedPtr<FTraceProviderBase> Provider : Providers)
		{
			const FName ProviderName = Provider->GetName();
			if (const uint64* CurrentUpdateId = UpdateIds.Find(ProviderName))
			{
				const uint64 LastUpdateId = Provider->GetLastUpdateId();
				if (*CurrentUpdateId != LastUpdateId)
				{
					UpdateFilterReason = EProcessReason::EntriesUpdated;
				}
			}
			else
			{
				UpdateFilterReason = EProcessReason::EntriesUpdated;
			}
		}

		if (UpdateFilterReason != EProcessReason::None)
		{
			ProcessEntries(UpdateFilterReason);
			if (UpdateFilterReason == EProcessReason::EntriesUpdated)
			{
				for (TSharedPtr<FTraceProviderBase> Provider : Providers)
				{
					const FName ProviderName = Provider->GetName();
					const uint64 LastUpdateId = Provider->GetLastUpdateId();
					UpdateIds.FindOrAdd(ProviderName) = LastUpdateId;
				}
			}

			RebuildFilteredEntriesListView();
			UpdateFilterReason = EProcessReason::None;
		}

#if WITH_EDITOR
		const bool bDrawDebug = IsDebugDrawEnabled();
		if (bDrawDebug)
		{
			FAudioDeviceManager* Manager = FAudioDeviceManager::Get();
			if (!Manager)
			{
				return;
			}

			if (!FilteredEntriesListView.IsValid())
			{
				return;
			}

			TArray<TSharedPtr<IDashboardDataViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();
			for (const TSharedPtr<IDashboardDataViewEntry>& SelectedEntry : SelectedItems)
			{
				if (SelectedEntry.IsValid())
				{
					Manager->IterateOverAllDevices([this, &SelectedEntry, InElapsed](::Audio::FDeviceId DeviceId, FAudioDevice* Device)
					{
						DebugDraw(InElapsed, *SelectedEntry.Get(), DeviceId);
					});
				}
			}
		}
#endif // WITH_EDITOR
	}

	FTraceTableDashboardViewFactory::FTraceTableDashboardViewFactory()
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("TraceTableDashboardViewFactory"), 0.0f, [this](float DeltaTime)
		{
			Tick(DeltaTime);
			return true;
		});
	}

	FTraceTableDashboardViewFactory::~FTraceTableDashboardViewFactory()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

	FText FSoundAssetDashboardEntry::GetDisplayName() const
	{
		return FText::FromString(FSoftObjectPath(Name).GetAssetName());
	}

	const UObject* FSoundAssetDashboardEntry::GetObject() const
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	UObject* FSoundAssetDashboardEntry::GetObject()
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	bool FSoundAssetDashboardEntry::IsValid() const
	{
		return PlayOrder != INDEX_NONE;
	}

	TSharedRef<SWidget> FTraceObjectTableDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& Column)
	{
		const IDashboardDataViewEntry& RowData = InRowData.Get();
		const FColumnData& ColumnData = GetColumns()[Column];
		const FText Label = ColumnData.GetDisplayValue(RowData);
		if (Label.IsEmpty())
		{
			return SNullWidget::NullWidget;
		}

		TSharedPtr<IDashboardDataViewEntry> RowDataPtr = InRowData;
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(2)
		[
			SNew(STextBlock).Text(Label)
			.ColorAndOpacity(GetRowColor(RowDataPtr))
			.OnDoubleClicked_Lambda([this, RowDataPtr](const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
			{
				if (GEditor)
				{
					TSharedPtr<IObjectDashboardEntry> ObjectData = StaticCastSharedPtr<IObjectDashboardEntry>(RowDataPtr);
					if (ObjectData.IsValid())
					{
						const UObject* Object = ObjectData->GetObject();
						if (Object && Object->IsAsset())
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

	FReply FTraceObjectTableDashboardViewFactory::OnDataRowKeyInput(const FGeometry& Geometry, const FKeyEvent& KeyEvent) const
	{
		if (FilteredEntriesListView.IsValid())
		{
			TArray<TSharedPtr<IDashboardDataViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();
			if (KeyEvent.GetKey() == EKeys::Enter)
			{
				for (const TSharedPtr<IDashboardDataViewEntry>& SelectedItem : SelectedItems)
				{
					if (SelectedItem.IsValid())
					{
						IObjectDashboardEntry& RowData = *StaticCastSharedPtr<IObjectDashboardEntry>(SelectedItem).Get();
						if (UObject* Object = RowData.GetObject())
						{
							if (Object && Object->IsAsset())
							{
								GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
							}
						}
					}
				}

				return FReply::Handled();
			}
		}
		return FReply::Unhandled();
	}

	TSharedRef<SWidget> FTraceObjectTableDashboardViewFactory::MakeWidget()
	{
		if (!DashboardWidget.IsValid())
		{
			DashboardWidget = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					MakeAssetMenuBar()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				FTraceTableDashboardViewFactory::MakeWidget()
			];

			if (FilteredEntriesListView.IsValid())
			{
				FilteredEntriesListView->SetSelectionMode(ESelectionMode::Multi);
			}
		}

		return DashboardWidget->AsShared();
	}

	TSharedRef<SWidget> FTraceObjectTableDashboardViewFactory::MakeAssetMenuBar() const
	{
		const FDashboardAssetCommands& Commands = FDashboardAssetCommands::Get();
		TSharedPtr<FUICommandList> ToolkitCommands = MakeShared<FUICommandList>();
		ToolkitCommands->MapAction(Commands.GetOpenCommand(), FExecuteAction::CreateLambda([this]() { OpenAsset(); }));
		ToolkitCommands->MapAction(Commands.GetBrowserSyncCommand(), FExecuteAction::CreateLambda([this]() { BrowseToAsset(); }));

		FToolBarBuilder ToolbarBuilder(ToolkitCommands, FMultiBoxCustomization::None);
		Commands.AddAssetCommands(ToolbarBuilder);

		return ToolbarBuilder.MakeWidget();
	}

	TArray<UObject*> FTraceObjectTableDashboardViewFactory::GetSelectedEditableAssets() const
	{
		TArray<UObject*> Objects;

		if (!FilteredEntriesListView.IsValid())
		{
			return Objects;
		}

		const TArray<TSharedPtr<IDashboardDataViewEntry>> Items = FilteredEntriesListView->GetSelectedItems();
		Algo::TransformIf(Items, Objects,
			[](const TSharedPtr<IDashboardDataViewEntry>& Item)
			{
				if (Item.IsValid())
				{
					IObjectDashboardEntry& RowData = *StaticCastSharedPtr<IObjectDashboardEntry>(Item).Get();
					if (UObject* Object = RowData.GetObject())
					{
						return Object && Object->IsAsset();
					}
				}

				return false;
			},
			[](const TSharedPtr<IDashboardDataViewEntry>& Item)
			{
				IObjectDashboardEntry& RowData = *StaticCastSharedPtr<IObjectDashboardEntry>(Item).Get();
				return RowData.GetObject();
			}
		);

		return Objects;
	}

	bool FTraceObjectTableDashboardViewFactory::OpenAsset() const
	{
		if (GEditor && FilteredEntriesListView.IsValid())
		{
			TArray<UObject*> Objects = GetSelectedEditableAssets();
			if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				return AssetSubsystem->OpenEditorForAssets(Objects);
			}
		}

		return false;
	}

	bool FTraceObjectTableDashboardViewFactory::BrowseToAsset() const
	{
		if (GEditor)
		{
			TArray<UObject*> EditableAssets = GetSelectedEditableAssets();
			GEditor->SyncBrowserToObjects(EditableAssets);
			return true;
		}

		return false;
	}

	void FSoundAttenuationVisualizer::Draw(float InDeltaTime, const FTransform& InTransform, const UObject& InObject, const UWorld& InWorld) const
	{
		if (LastObjectId != InObject.GetUniqueID())
		{
			ShapeDetailsMap.Reset();
			if (const USoundCue* SoundCue = Cast<const USoundCue>(&InObject))
			{
				TArray<const USoundNodeAttenuation*> AttenuationNodes;
				SoundCue->RecursiveFindAttenuation(SoundCue->FirstNode, AttenuationNodes);
				for (const USoundNodeAttenuation* Node : AttenuationNodes)
				{
					if (Node)
					{
						if (const FSoundAttenuationSettings* AttenuationSettingsToApply = Node->GetAttenuationSettingsToApply())
						{
							AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
						}
					}
				}
			}
			else if (const USoundBase* SoundBase = Cast<USoundBase>(&InObject))
			{
				if (const FSoundAttenuationSettings* Settings = SoundBase->GetAttenuationSettingsToApply())
				{
					Settings->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
				}
			}
			else
			{
				return;
			}
		}
		LastObjectId = InObject.GetUniqueID();

		for (const TPair<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& Pair : ShapeDetailsMap)
		{
			const FBaseAttenuationSettings::AttenuationShapeDetails& ShapeDetails = Pair.Value;
			switch (Pair.Key)
			{
				case EAttenuationShape::Sphere:
				{
					if (ShapeDetails.Falloff > 0.f)
					{
						DrawDebugSphere(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, 10, Color);
						DrawDebugSphere(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X, 10, Color);
					}
					else
					{
						DrawDebugSphere(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X, 10, Color);
					}
					break;
				}

				case EAttenuationShape::Box:
				{
					if (ShapeDetails.Falloff > 0.f)
					{
						DrawDebugBox(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents + FVector(ShapeDetails.Falloff), InTransform.GetRotation(), Color);
						DrawDebugBox(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents, InTransform.GetRotation(), Color);
					}
					else
					{
						DrawDebugBox(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents, InTransform.GetRotation(), Color);
					}
					break;
				}

				case EAttenuationShape::Capsule:
				{
					if (ShapeDetails.Falloff > 0.f)
					{
						DrawDebugCapsule(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, ShapeDetails.Extents.Y + ShapeDetails.Falloff, InTransform.GetRotation(), Color);
						DrawDebugCapsule(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, InTransform.GetRotation(), Color);
					}
					else
					{
						DrawDebugCapsule(&InWorld, InTransform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, InTransform.GetRotation(), Color);
					}
					break;
				}

				case EAttenuationShape::Cone:
				{
					const FVector Origin = InTransform.GetTranslation() - (InTransform.GetUnitAxis(EAxis::X) * ShapeDetails.ConeOffset);

					if (ShapeDetails.Falloff > 0.f || ShapeDetails.Extents.Z > 0.f)
					{
						const float OuterAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y + ShapeDetails.Extents.Z);
						const float InnerAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
						DrawDebugCone(&InWorld, Origin, InTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.Falloff + ShapeDetails.ConeOffset, OuterAngle, OuterAngle, 10, Color);
						DrawDebugCone(&InWorld, Origin, InTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, InnerAngle, InnerAngle, 10, Color);
					}
					else
					{
						const float Angle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
						DrawDebugCone(&InWorld, Origin, InTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, Angle, Angle, 10, Color);
					}

					if (!FMath::IsNearlyZero(ShapeDetails.ConeSphereRadius, UE_KINDA_SMALL_NUMBER))
					{
						if (ShapeDetails.ConeSphereFalloff > 0.f)
						{

							DrawDebugSphere(&InWorld, Origin, ShapeDetails.ConeSphereRadius + ShapeDetails.ConeSphereFalloff, 10, Color);
							DrawDebugSphere(&InWorld, Origin, ShapeDetails.ConeSphereRadius, 10, Color);
						}
						else
						{
							DrawDebugSphere(&InWorld, Origin, ShapeDetails.ConeSphereRadius, 10, Color);
						}
					}

					break;
				}

				default:
				{
					break;
				}
			}
		}
	}
} // namespace UE::Audio::Insights

#undef LOCTEXT_NAMESPACE
