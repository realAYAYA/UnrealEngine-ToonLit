// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SRenderGridJobList.h"

#include "UI/Components/SRenderGridDragHandle.h"
#include "UI/Components/SRenderGridEditableTextBlock.h"
#include "UI/Components/SRenderGridFileSelectorTextBlock.h"
#include "RenderGrid/RenderGrid.h"
#include "RenderGrid/RenderGridQueue.h"
#include "IRenderGridEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineQueue.h"
#include "MovieScene.h"
#include "PropertyCustomizationHelpers.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "SRenderGridJobList"


namespace UE::RenderGrid::Private::FRenderGridJobListColumns
{
	const FName DragDropHandle = TEXT("DragDropHandle");
	const FName IsEnabled = TEXT("IsEnabled");
	const FName JobId = TEXT("JobId");
	const FName JobName = TEXT("JobName");
	const FName OutputDirectory = TEXT("OutputDirectory");
	const FName RenderPreset = TEXT("RenderPreset");
	const FName StartFrame = TEXT("StartFrame");
	const FName EndFrame = TEXT("EndFrame");
	const FName Resolution = TEXT("Resolution");
	const FName Tags = TEXT("Tags");
	const FName Duration = TEXT("Duration");
	const FName RenderingStatus = TEXT("Status");
}


void UE::RenderGrid::Private::SRenderGridJobList::Tick(const FGeometry&, const double, const float)
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		if (RenderGridWeakPtr != BlueprintEditor->GetInstance())
		{
			Refresh();
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void UE::RenderGrid::Private::SRenderGridJobList::Construct(const FArguments& InArgs, TSharedPtr<IRenderGridEditor> InBlueprintEditor)
{
	BlueprintEditorWeakPtr = InBlueprintEditor;

	Refresh();
	InBlueprintEditor->OnRenderGridJobCreated().AddSP(this, &SRenderGridJobList::OnRenderGridJobCreated);
	InBlueprintEditor->OnRenderGridChanged().AddSP(this, &SRenderGridJobList::Refresh);
	InBlueprintEditor->OnRenderGridBatchRenderingStarted().AddSP(this, &SRenderGridJobList::OnBatchRenderingStarted);
	InBlueprintEditor->OnRenderGridBatchRenderingFinished().AddSP(this, &SRenderGridJobList::OnBatchRenderingFinished);
	FCoreUObjectDelegates::OnObjectModified.AddSP(this, &SRenderGridJobList::OnObjectModified);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(SHorizontalBox)

			// Search Box
			+ SHorizontalBox::Slot()
			.Padding(4.f, 2.f)
			[
				SAssignNew(RenderGridSearchBox, SSearchBox)
				.HintText(LOCTEXT("Search_HintText", "Search Tags | Text"))
				.OnTextChanged(this, &SRenderGridJobList::OnSearchBarTextChanged)
			]

			// Filters
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 2.f, 2.f, 2.f)
			[
				SNew(SComboButton)
				.ComboButtonStyle(FAppStyle::Get(), "GenericFilters.ComboButtonStyle")
				.ForegroundColor(FLinearColor::White)
				.ToolTipText(LOCTEXT("Filters_Tooltip", "Filter options for the Job List."))
				.HasDownArrow(true)
				.ContentPadding(0.f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "GenericFilters.TextStyle")
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.12"))
					.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
				]
			]
		]

		// Job List
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.f)
			[
				SAssignNew(RenderGridJobListWidget, SListView<URenderGridJob*>)
				.ItemHeight(20.0f)
				.OnGenerateRow(this, &SRenderGridJobList::HandleJobListGenerateRow)
				.OnSelectionChanged(this, &SRenderGridJobList::HandleJobListSelectionChanged)
				.SelectionMode(ESelectionMode::Multi)
				.ClearSelectionOnClick(false)
				.ListItemsSource(&RenderGridJobs)
				.HeaderRow(
					SNew(SHeaderRow)

					+ SHeaderRow::Column(FRenderGridJobListColumns::DragDropHandle)
					.DefaultLabel(LOCTEXT("JobListDragDropHandleColumnHeader", ""))
					.FixedWidth(36.0f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::IsEnabled)
					.DefaultLabel(LOCTEXT("JobListIsEnabledColumnHeader", "Enabled"))
					.FixedWidth(30.0f) //55.0f for text : "Enabled"
					[
						SAssignNew(RenderGridJobEnabledHeaderCheckbox, SCheckBox)
						.IsChecked(true)
						.OnCheckStateChanged(this, &SRenderGridJobList::OnHeaderCheckboxToggled)
					]

					+ SHeaderRow::Column(FRenderGridJobListColumns::JobId)
					.DefaultLabel(LOCTEXT("JobListIDColumnHeader", "Job ID"))
					.FillWidth(0.3f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::JobName)
					.DefaultLabel(LOCTEXT("JobListNameColumnHeader", "Job Name"))
					.FillWidth(0.3f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::OutputDirectory)
					.DefaultLabel(LOCTEXT("JobListOutDirColumnHeader", "Output Directory"))
					.FillWidth(0.7f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::RenderPreset)
					.DefaultLabel(LOCTEXT("JobListRenderPresetColumnHeader", "Render Preset"))
					.FillWidth(0.5f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::StartFrame)
					.DefaultLabel(LOCTEXT("JobListStartFrameColumnHeader", "Start Frame"))
					.FixedWidth(80.0f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::EndFrame)
					.DefaultLabel(LOCTEXT("JobListEndFrameColumnHeader", "End Frame"))
					.FixedWidth(80.0f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::Resolution)
					.DefaultLabel(LOCTEXT("JobListResolutionColumnHeader", "Resolution"))
					.FillWidth(0.7f)

					+ SHeaderRow::Column(FRenderGridJobListColumns::Duration)
					.DefaultLabel(LOCTEXT("JobListEstDurColumnHeader", "Est Duration"))
					.FixedWidth(120.0f)
				)
			]
		]
	];

	Refresh();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void UE::RenderGrid::Private::SRenderGridJobList::OnRenderGridJobCreated(URenderGridJob* Job)
{
	if (!RenderGridJobEnabledHeaderCheckbox.IsValid())
	{
		return;
	}
	Job->SetIsEnabled(RenderGridJobEnabledHeaderCheckbox->GetCheckedState() != ECheckBoxState::Unchecked);
}

void UE::RenderGrid::Private::SRenderGridJobList::OnHeaderCheckboxToggled(ECheckBoxState State)
{
	bool bRefresh = false;

	for (URenderGridJob* Job : RenderGridJobs)
	{
		Job->SetIsEnabled(State == ECheckBoxState::Checked);
		bRefresh = true;
	}

	if (bRefresh)
	{
		Refresh();
	}
}

ECheckBoxState UE::RenderGrid::Private::SRenderGridJobList::GetDesiredHeaderEnabledCheckboxState()
{
	ECheckBoxState State = ECheckBoxState::Checked;
	bool bFirstJob = true;
	for (URenderGridJob* Job : RenderGridJobs)
	{
		ECheckBoxState JobState = (Job->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		if (bFirstJob)
		{
			State = JobState;
			bFirstJob = false;
		}
		else if (State != JobState)
		{
			return ECheckBoxState::Undetermined;
		}
	}
	return State;
}

void UE::RenderGrid::Private::SRenderGridJobList::AddRenderStatusColumn()
{
	if (!RenderGridJobListWidget.IsValid())
	{
		return;
	}
	RenderGridJobListWidget->GetHeaderRow()->AddColumn(SHeaderRow::Column(FRenderGridJobListColumns::RenderingStatus)
		.DefaultLabel(LOCTEXT("JobListRenderStatusColumnHeader", "Render Status"))
		.FixedWidth(110.0f));
}

void UE::RenderGrid::Private::SRenderGridJobList::RemoveRenderStatusColumn()
{
	if (!RenderGridJobListWidget.IsValid())
	{
		return;
	}
	RenderGridJobListWidget->GetHeaderRow()->RemoveColumn(FRenderGridJobListColumns::RenderingStatus);
}


void UE::RenderGrid::Private::SRenderGridJobList::OnObjectModified(UObject* Object)
{
	if (Cast<UMoviePipelineOutputSetting>(Object) || Cast<UMovieSceneSequence>(Object) || Cast<UMovieScene>(Object) || Cast<UMovieSceneTrack>(Object) || Cast<UMovieSceneSection>(Object) || Cast<UMovieSceneSubTrack>(Object) || Cast<UMovieSceneSubSection>(Object))
	{
		Refresh();
	}
}


void UE::RenderGrid::Private::SRenderGridJobList::Refresh()
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		const bool bIsBatchRendering = BlueprintEditor->IsBatchRendering();// show all jobs during a batch render, ignore the search bar
		const FString SearchBarContent = (!RenderGridSearchBox.IsValid() ? TEXT("") : RenderGridSearchBox->GetText().ToString());

		RenderGridJobs.Empty();
		RenderGridWeakPtr = BlueprintEditor->GetInstance();
		if (URenderGrid* Grid = RenderGridWeakPtr.Get(); IsValid(Grid))
		{
			for (URenderGridJob* Job : Grid->GetRenderGridJobs())
			{
				if (bIsBatchRendering || Job->MatchesSearchTerm(SearchBarContent))
				{
					RenderGridJobs.Add(Job);
				}
			}
		}

		RefreshHeaderEnabledCheckbox();

		RemoveRenderStatusColumn();
		if (bIsBatchRendering)
		{
			AddRenderStatusColumn();
		}

		if (RenderGridJobListWidget.IsValid())
		{
			RenderGridJobListWidget->RebuildList(); // rebuild is needed (instead of using RequestListRefresh()), because otherwise it won't show the changes made to the URenderGridJob variables

			TArray<URenderGridJob*> SelectedJobs = BlueprintEditor->GetSelectedRenderGridJobs().FilterByPredicate([this](URenderGridJob* Job)
			{
				return IsValid(Job) && RenderGridJobs.Contains(Job);
			});
			RenderGridJobListWidget->ClearSelection();
			RenderGridJobListWidget->SetItemSelection(SelectedJobs, true);
			BlueprintEditor->SetSelectedRenderGridJobs(SelectedJobs);
		}
	}
}

void UE::RenderGrid::Private::SRenderGridJobList::RefreshHeaderEnabledCheckbox()
{
	if (!RenderGridJobEnabledHeaderCheckbox.IsValid())
	{
		return;
	}
	RenderGridJobEnabledHeaderCheckbox->SetIsChecked(GetDesiredHeaderEnabledCheckboxState());
}

TSharedRef<ITableRow> UE::RenderGrid::Private::SRenderGridJobList::HandleJobListGenerateRow(URenderGridJob* Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> BaseThis = AsShared();
	TSharedPtr<SRenderGridJobList> This = StaticCastSharedPtr<SRenderGridJobList>(BaseThis);

	return SNew(SRenderGridJobListTableRow, OwnerTable, BlueprintEditorWeakPtr, Item, This);
}

void UE::RenderGrid::Private::SRenderGridJobList::HandleJobListSelectionChanged(URenderGridJob* Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Type::Direct)
	{
		return;
	}
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		BlueprintEditor->SetSelectedRenderGridJobs(RenderGridJobListWidget->GetSelectedItems());
	}
}


void UE::RenderGrid::Private::SRenderGridJobListTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TWeakPtr<IRenderGridEditor> InBlueprintEditor, URenderGridJob* InRenderGridJob, const TSharedPtr<SRenderGridJobList>& InJobListWidget)
{
	BlueprintEditorWeakPtr = InBlueprintEditor;
	RenderGridJob = InRenderGridJob;
	JobListWidget = InJobListWidget;

	SMultiColumnTableRow<URenderGridJob*>::Construct(FSuperRowType::FArguments()
		.OnCanAcceptDrop(this, &SRenderGridJobListTableRow::OnCanAcceptDrop)
		.OnAcceptDrop(this, &SRenderGridJobListTableRow::OnAcceptDrop),
		InOwnerTableView);
}

FReply UE::RenderGrid::Private::SRenderGridJobListTableRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return SMultiColumnTableRow<URenderGridJob*>::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	{
		MenuBuilder.BeginSection("RenderGridJobListRowContextMenuSection", LOCTEXT("RenderGridJobListRowContextMenuHeading", "Options"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DuplicateRenderGridJobListRowLabel", "Duplicate"),
				LOCTEXT("DuplicateRenderGridJobListTooltip", "Creates a copy of the job and adds it to the grid."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
				FUIAction(FExecuteAction::CreateSP(this, &SRenderGridJobListTableRow::DuplicateJob))
			);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteRenderGridJobListRowLabel", "Delete"),
				LOCTEXT("DeleteRenderGridJobListTooltip", "Removes the job from the grid."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction(FExecuteAction::CreateSP(this, &SRenderGridJobListTableRow::DeleteJob))
			);
		}
		MenuBuilder.EndSection();
	}
	TSharedPtr<SWidget> MenuContent = MenuBuilder.MakeWidget();

	if (MenuContent.IsValid() && (MouseEvent.GetEventPath() != nullptr))
	{
		FWidgetPath WidgetPath = *MouseEvent.GetEventPath();
		FSlateApplication::Get().PushMenu(WidgetPath.Widgets.Last().Widget, WidgetPath, MenuContent.ToSharedRef(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
	return FReply::Handled();
}

TOptional<EItemDropZone> UE::RenderGrid::Private::SRenderGridJobListTableRow::OnCanAcceptDrop(const FDragDropEvent& InEvent, EItemDropZone InItemDropZone, URenderGridJob* InJob)
{
	if (BlueprintEditorWeakPtr.IsValid() && InEvent.GetOperationAs<FRenderGridJobListTableRowDragDropOp>())
	{
		if (InItemDropZone == EItemDropZone::OntoItem)
		{
			return EItemDropZone::BelowItem;
		}
		return InItemDropZone;
	}
	return TOptional<EItemDropZone>();
}

FReply UE::RenderGrid::Private::SRenderGridJobListTableRow::OnAcceptDrop(const FDragDropEvent& InEvent, EItemDropZone InItemDropZone, URenderGridJob* InJob)
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		if (const TSharedPtr<FRenderGridJobListTableRowDragDropOp> DragDropOp = InEvent.GetOperationAs<FRenderGridJobListTableRowDragDropOp>())
		{
			if (URenderGrid* Instance = BlueprintEditor->GetInstance(); IsValid(Instance))
			{
				if (Instance->ReorderRenderGridJob(DragDropOp->GetJob(), InJob, (InItemDropZone != EItemDropZone::AboveItem)))
				{
					BlueprintEditor->MarkAsModified();
					BlueprintEditor->OnRenderGridChanged().Broadcast();
					return FReply::Handled();
				}
			}
		}
	}
	return FReply::Unhandled();
}

TSharedRef<SWidget> UE::RenderGrid::Private::SRenderGridJobListTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!IsValid(RenderGridJob))
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == FRenderGridJobListColumns::DragDropHandle)
	{
		return SNew(SBox)
			.Padding(FMargin(0.0f, 2.0f, 2.0f, 2.0f))
			[
				SNew(SScaleBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::Both)
				[
					SNew(SRenderGridDragHandle<FRenderGridJobListTableRowDragDropOp>, RenderGridJob)
					.Widget(SharedThis(this))
				]
			];
	}
	else if (ColumnName == FRenderGridJobListColumns::IsEnabled)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(RenderGridJob->GetIsEnabled())
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					RenderGridJob->SetIsEnabled(State == ECheckBoxState::Checked);
					if (JobListWidget.IsValid())
					{
						JobListWidget->RefreshHeaderEnabledCheckbox();
					}
					if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
					{
						BlueprintEditor->MarkAsModified();
					}
				})
			];
	}
	else if (ColumnName == FRenderGridJobListColumns::JobId)
	{
		return SNew(SRenderGridEditableTextBlock)
			.Text(FText::FromString(RenderGridJob->GetJobId()))
			.OnTextCommitted_Lambda([this](const FText& InLabel, ETextCommit::Type InCommitInfo) -> FText
			{
				const FString OldJobId = RenderGridJob->GetJobId();
				const FString NewJobId = URenderGridJob::PurgeJobIdOrReturnEmptyString(InLabel.ToString());
				if (NewJobId.IsEmpty() || (RenderGridJob->GetJobId() == NewJobId))
				{
					return FText::FromString(OldJobId);
				}

				if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
				{
					if (URenderGrid* Grid = BlueprintEditor->GetInstance(); IsValid(Grid))
					{
						if (Grid->DoesJobIdExist(NewJobId))
						{
							const FText TitleText = LOCTEXT("JobIdNotUniqueTitle", "Duplicate Job IDs");
							FMessageDialog::Open(
								EAppMsgType::Ok,
								FText::Format(LOCTEXT("JobIdNotUniqueMessage", "Job ID \"{0}\" is not unique."), FText::FromString(NewJobId)),
								&TitleText);
							return FText::FromString(OldJobId);
						}

						RenderGridJob->SetJobId(NewJobId);
						BlueprintEditor->MarkAsModified();
						return FText::FromString(RenderGridJob->GetJobId());
					}
				}
				return FText::FromString(OldJobId);
			});
	}
	else if (ColumnName == FRenderGridJobListColumns::JobName)
	{
		return SNew(SRenderGridEditableTextBlock)
			.Text(FText::FromString(RenderGridJob->GetJobName()))
			.OnTextCommitted_Lambda([this](const FText& InLabel, ETextCommit::Type InCommitInfo) -> FText
			{
				RenderGridJob->SetJobName(InLabel.ToString());
				if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
				{
					BlueprintEditor->MarkAsModified();
				}
				return FText::FromString(RenderGridJob->GetJobName());
			});
	}
	else if (ColumnName == FRenderGridJobListColumns::OutputDirectory)
	{
		return SNew(SRenderGridFileSelectorTextBlock)
			.Text(FText::FromString(RenderGridJob->GetOutputDirectoryForDisplay()))
			.FolderPath_Lambda([this]() -> FString
			{
				return RenderGridJob->GetOutputDirectory();
			})
			.OnTextCommitted_Lambda([this](const FText& InLabel, ETextCommit::Type InCommitInfo) -> FText
			{
				RenderGridJob->SetOutputDirectory(InLabel.ToString());
				if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
				{
					BlueprintEditor->MarkAsModified();
				}
				return FText::FromString(RenderGridJob->GetOutputDirectoryForDisplay());
			});
	}
	else if (ColumnName == FRenderGridJobListColumns::RenderPreset)
	{
		return SNew(SObjectPropertyEntryBox)
			.AllowedClass(UMoviePipelineMasterConfig::StaticClass())
			.ObjectPath_Lambda([this]() -> FString
			{
				if (UMoviePipelineMasterConfig* Preset = RenderGridJob->GetRenderPreset(); IsValid(Preset))
				{
					return Preset->GetPathName();
				}
				return FString();
			})
			.OnObjectChanged_Lambda([this](const FAssetData& AssetData) -> void
			{
				RenderGridJob->SetRenderPreset(nullptr);
				if (UObject* AssetDataAsset = AssetData.GetAsset(); IsValid(AssetDataAsset))
				{
					if (UMoviePipelineMasterConfig* Preset = Cast<UMoviePipelineMasterConfig>(AssetDataAsset))
					{
						RenderGridJob->SetRenderPreset(Preset);
					}
				}
				if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
				{
					BlueprintEditor->MarkAsModified();
					BlueprintEditor->OnRenderGridChanged().Broadcast();
				}
			})
			.AllowClear(true)
			.DisplayUseSelected(true)
			.DisplayBrowse(true)
			.DisplayThumbnail(false);
	}
	else if ((ColumnName == FRenderGridJobListColumns::StartFrame) || (ColumnName == FRenderGridJobListColumns::EndFrame))
	{
		FText Text;
		if (TOptional<int32> Frame = ((ColumnName == FRenderGridJobListColumns::StartFrame) ? RenderGridJob->GetStartFrame() : RenderGridJob->GetEndFrame()))
		{
			Text = FText::AsNumber(*Frame);
		}
		return SNew(SBox)
			.VAlign(VAlign_Center)
			//.HAlign(HAlign_Right)
			.Padding(FMargin(10, 0))
			[
				SNew(STextBlock).Text(Text)
			];
	}
	else if (ColumnName == FRenderGridJobListColumns::Tags)
	{
		//TODO: add support for tags
	}
	else if (ColumnName == FRenderGridJobListColumns::Resolution)
	{
		FIntPoint Resolution = RenderGridJob->GetOutputResolution();

		FNumberFormattingOptions NumberFormattingOptions;
		NumberFormattingOptions.UseGrouping = false;

		FText ResolutionFormat = LOCTEXT("Resolution", "{Width} x {Height}");
		FFormatNamedArguments ResolutionArguments;
		ResolutionArguments.Add(TEXT("Width"), FText::AsNumber(Resolution.X, &NumberFormattingOptions));
		ResolutionArguments.Add(TEXT("Height"), FText::AsNumber(Resolution.Y, &NumberFormattingOptions));
		FText Text = FText::Format(ResolutionFormat, ResolutionArguments);

		return SNew(SBox)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10, 0))
			[
				SNew(STextBlock).Text(Text)
			];
	}
	else if (ColumnName == FRenderGridJobListColumns::Duration)
	{
		FText Text;
		if (TOptional<double> Duration = RenderGridJob->GetDurationInSeconds())
		{
			FTimespan Timespan = FTimespan::FromSeconds(*Duration);
			int32 Hours = static_cast<int32>(Timespan.GetTotalHours());
			int32 Minutes = Timespan.GetMinutes();
			int32 Seconds = Timespan.GetSeconds();

			FNumberFormattingOptions NumberFormattingOptions;
			NumberFormattingOptions.MinimumIntegralDigits = 2;
			NumberFormattingOptions.MaximumIntegralDigits = 2;

			FText TimespanFormat = NSLOCTEXT("Timespan", "Format_HoursMinutesSeconds", "{Hours}:{Minutes}:{Seconds}");
			FFormatNamedArguments TimeArguments;
			TimeArguments.Add(TEXT("Hours"), Hours);
			TimeArguments.Add(TEXT("Minutes"), FText::AsNumber(Minutes, &NumberFormattingOptions));
			TimeArguments.Add(TEXT("Seconds"), FText::AsNumber(Seconds, &NumberFormattingOptions));
			Text = FText::Format(TimespanFormat, TimeArguments);
		}
		return SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Text)
			];
	}
	else if (ColumnName == FRenderGridJobListColumns::RenderingStatus)
	{
		return SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return GetRenderStatusText();
				})
			];
	}
	return SNullWidget::NullWidget;
}

void UE::RenderGrid::Private::SRenderGridJobListTableRow::DuplicateJob()
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		if (URenderGrid* RenderGrid = BlueprintEditor->GetInstance(); IsValid(RenderGrid))
		{
			RenderGrid->DuplicateAndAddRenderGridJob(RenderGridJob);
			BlueprintEditor->MarkAsModified();
			BlueprintEditor->OnRenderGridChanged().Broadcast();
		}
	}
}

void UE::RenderGrid::Private::SRenderGridJobListTableRow::DeleteJob()
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		if (URenderGrid* RenderGrid = BlueprintEditor->GetInstance(); IsValid(RenderGrid))
		{
			RenderGrid->RemoveRenderGridJob(RenderGridJob);
			BlueprintEditor->MarkAsModified();
			BlueprintEditor->OnRenderGridChanged().Broadcast();
		}
	}
}

FText UE::RenderGrid::Private::SRenderGridJobListTableRow::GetRenderStatusText() const
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		if (URenderGridQueue* RenderQueue = BlueprintEditor->GetBatchRenderQueue(); IsValid(RenderQueue))
		{
			return FText::FromString(RenderQueue->GetJobStatus(RenderGridJob));
		}
	}
	return FText();
}


#undef LOCTEXT_NAMESPACE
