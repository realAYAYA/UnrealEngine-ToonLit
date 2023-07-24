// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotBrowser.cpp: Implements the SScreenShotBrowser class.
=============================================================================*/

#include "Widgets/SScreenShotBrowser.h"
#include "HAL/FileManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/SScreenComparisonRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Models/ScreenComparisonModel.h"
#include "Misc/FeedbackContext.h"
#include "Styling/AppStyle.h"
#include "Modules/ModuleManager.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "SPositiveActionButton.h"
#include "SNegativeActionButton.h"

#define LOCTEXT_NAMESPACE "ScreenshotComparison"

void SScreenShotBrowser::Construct( const FArguments& InArgs,  IScreenShotManagerRef InScreenShotManager  )
{
	ScreenShotManager = InScreenShotManager;

	// Default path. This can be set by the UI or will be updated when a new report is generated.
	FString ReportPath = FPaths::AutomationReportsDir();

	// Ensure the report path exists since we will register for change notifications
	if (!IFileManager::Get().DirectoryExists(*ReportPath))
	{
		IFileManager::Get().MakeDirectory(*ReportPath, true);
	}

	ComparisonRoot = FPaths::ConvertRelativePathToFull(ReportPath);
	bReportsChanged = true;
	// Show Errors and New images, but hide Successful images (ie unchanged ones) because fails and new images are the actionable items
	bDisplayingSuccess = false;
	bDisplayingError = true;
	bDisplayingNew = true;
	ReportFilterString = FString();

	FModuleManager::Get().LoadModuleChecked(FName("ImageWrapper"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SDirectoryPicker)
				.Directory(ComparisonRoot)
				.OnDirectoryChanged(this, &SScreenShotBrowser::OnDirectoryChanged)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			.Padding(10.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("ScreenshotFilterHint", "Filter Reports"))
					.Text(FText::FromString(ReportFilterString))
					.OnTextCommitted(this, &SScreenShotBrowser::OnFilterStringCommitted)
					
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.HAlign(HAlign_Center)
					.IsChecked(bDisplayingSuccess ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SScreenShotBrowser::DisplaySuccess_OnCheckStateChanged)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
					.Text(LOCTEXT("DisplaySuccess", "Show Passed"))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(2.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.HAlign(HAlign_Center)
						.IsChecked( bDisplayingError ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.OnCheckStateChanged(this, &SScreenShotBrowser::DisplayError_OnCheckStateChanged)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("DisplayErrors", "Show Fails"))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.HAlign(HAlign_Center)
						.IsChecked(bDisplayingNew ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.OnCheckStateChanged(this, &SScreenShotBrowser::DisplayNew_OnCheckStateChanged)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("DisplayNew", "Show New"))
					]
				]
			]			
		]

		+ SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SNew(SPositiveActionButton)
					.Text(LOCTEXT("AddAllNewReports", "Add All New Reports"))
					.ToolTipText(LOCTEXT("AddAllNewReportsTooltip", "Adds all new screenshots contained in the reports."))
					.IsEnabled_Lambda([this]() -> bool
						{
							return ComparisonList.Num() > 0 && ISourceControlModule::Get().IsEnabled();
						})
					.OnClicked_Lambda([this]()
						{
							for (int Entry = ComparisonList.Num() - 1; Entry >= 0; --Entry)
							{
								TSharedPtr<FScreenComparisonModel> Model = ComparisonList[Entry];
								const FImageComparisonResult& Comparison = Model->Report.GetComparisonResult();

								if (CanAddNewReportResult(Comparison))
								{
									ComparisonList.RemoveAt(Entry);
									Model->AddNew();

									// Avoid thrashing P4
									const float SleepBetweenOperations = 0.005f;
									FPlatformProcess::Sleep(SleepBetweenOperations);
								}
							}

							return FReply::Handled();
						})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SNew(SNegativeActionButton)
					.ActionButtonStyle(EActionButtonStyle::Warning)
					.Text(LOCTEXT("ReplaceAllReports", "Replace All Reports"))
					.ToolTipText(LOCTEXT("ReplaceAllReportsTooltip", "Replaces all screenshots containing a different result in the reports."))
					.IsEnabled_Lambda([this]() -> bool
						{
							return ComparisonList.Num() > 0 && ISourceControlModule::Get().IsEnabled();
						})
					.OnClicked_Lambda([this]()
						{
							for (int Entry = ComparisonList.Num() - 1; Entry >= 0; --Entry)
							{
								TSharedPtr<FScreenComparisonModel> Model = ComparisonList[Entry];
								const FImageComparisonResult& Comparison = Model->Report.GetComparisonResult();

								if (!CanAddNewReportResult(Comparison))
								{
									ComparisonList.RemoveAt(Entry);
									Model->Replace();

									// Avoid thrashing P4
									const float SleepBetweenOperations = 0.005f;
									FPlatformProcess::Sleep(SleepBetweenOperations);
								}
							}

							return FReply::Handled();
						})
					]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SNew(SNegativeActionButton)
					.Text(LOCTEXT("DeleteAllReports", "Delete All Reports"))
					.ToolTipText(LOCTEXT("DeleteAllReportsTooltip", "Deletes all the current reports.  Reports are not removed unless the user resolves them, \nso if you just want to reset the state of the reports, clear them here and then re-run the tests."))
					.IsEnabled_Lambda([this]() -> bool
						{
							return ComparisonList.Num() > 0;
						})
					.OnClicked_Lambda([this]()
						{
							while (ComparisonList.Num() > 0)
							{
								TSharedPtr<FScreenComparisonModel> Model = ComparisonList.Pop();
								Model->Complete(true);
							}

							return FReply::Handled();
						})
				]
			]
		]
		
		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )
		[
			SAssignNew(ComparisonView, SListView< TSharedPtr<FScreenComparisonModel> >)
			.ListItemsSource(&ComparisonList)
			.OnGenerateRow(this, &SScreenShotBrowser::OnGenerateWidgetForScreenResults)
			.SelectionMode(ESelectionMode::None)
			.HeaderRow
			(
				SNew(SHeaderRow)

				+ SHeaderRow::Column("Name")
				.DefaultLabel(LOCTEXT("ColumnHeader_Name", "Name"))
				.FillWidth(1.0f)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Date")
				.DefaultLabel(LOCTEXT("ColumnHeader_Date", "Date"))
				.FixedWidth(120)
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Platform")
				.DefaultLabel(LOCTEXT("ColumnHeader_Platform", "Platform"))
				.FixedWidth(120)
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Delta")
				.DefaultLabel(LOCTEXT("ColumnHeader_Delta", "Local | Global Delta"))
				.FixedWidth(120)
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Preview")
				.DefaultLabel(LOCTEXT("ColumnHeader_Preview", "Preview"))
				.FixedWidth(500)
				.HAlignHeader(HAlign_Left)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)
			)
		]
	];

	RefreshDirectoryWatcher();
}

SScreenShotBrowser::~SScreenShotBrowser()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));

	if ( DirectoryWatchingHandle.IsValid() )
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(ComparisonRoot, DirectoryWatchingHandle);
	}
}

void SScreenShotBrowser::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if ( bReportsChanged )
	{
		RebuildTree();
	}
}

void SScreenShotBrowser::OnDirectoryChanged(const FString& Directory)
{
	ComparisonRoot = Directory;

	RefreshDirectoryWatcher();

	bReportsChanged = true;
}

void SScreenShotBrowser::RefreshDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	
	if ( DirectoryWatchingHandle.IsValid() )
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(ComparisonRoot, DirectoryWatchingHandle);
	}

	DirectoryWatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(ComparisonRoot, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &SScreenShotBrowser::OnReportsChanged), DirectoryWatchingHandle, IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
}

void SScreenShotBrowser::OnReportsChanged(const TArray<struct FFileChangeData>& /*FileChanges*/)
{
	bReportsChanged = true;
}

TSharedRef<ITableRow> SScreenShotBrowser::OnGenerateWidgetForScreenResults(TSharedPtr<FScreenComparisonModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Create the row widget.
	return
		SNew(SScreenComparisonRow, OwnerTable)
		.ScreenshotManager(ScreenShotManager)
		.ComparisonDirectory(ComparisonRoot)
		.ComparisonResult(InItem);
}

void SScreenShotBrowser::DisplaySuccess_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	bDisplayingSuccess = (NewRadioState == ECheckBoxState::Checked);
	bReportsChanged = true;
}

void SScreenShotBrowser::DisplayError_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	bDisplayingError = (NewRadioState == ECheckBoxState::Checked);
	bReportsChanged = true;
}

void SScreenShotBrowser::DisplayNew_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	bDisplayingNew = (NewRadioState == ECheckBoxState::Checked);
	bReportsChanged = true;
}

void SScreenShotBrowser::OnFilterStringCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FString InString = InText.ToString();
	if (ReportFilterString.Compare(InString, ESearchCase::IgnoreCase) != 0)
	{
		ReportFilterString = InString;
		bReportsChanged = true;
	}
}

void SScreenShotBrowser::RebuildTree()
{
	bReportsChanged = false;
	ComparisonList.Reset();

	if ( ScreenShotManager->OpenComparisonReports(ComparisonRoot, CurrentReports) )
	{
		//Sort by what will resolve down to the Screenshots' names
		CurrentReports.Sort([](const FComparisonReport& LHS, const FComparisonReport& RHS) { return LHS.GetReportPath().Compare(RHS.GetReportPath()) < 0; });

		for ( const FComparisonReport& Report : CurrentReports )
		{
			const FImageComparisonResult& Comparison = Report.GetComparisonResult();

			bool IsNew = Comparison.IsNew();
			bool IsFail = !Comparison.AreSimilar();
			bool IsPass = !IsNew && !IsFail;

			if (IsPass && !bDisplayingSuccess)
			{
				continue;
			}

			if (IsNew && !bDisplayingNew)
			{
				continue;
			}

			if (IsFail && !bDisplayingError)
			{
				continue;
			}

			if (ReportFilterString.Len() && !Report.GetReportPath().Contains(ReportFilterString, ESearchCase::IgnoreCase))
			{
				continue;
			}

			TSharedPtr<FScreenComparisonModel> Model = MakeShared<FScreenComparisonModel>(Report);
			Model->OnComplete.AddLambda([this, Model] () {
				ComparisonList.Remove(Model);
				ComparisonView->RequestListRefresh();
			});

			ComparisonList.Add(Model);
		}
	}

	ComparisonView->RequestListRefresh();
}

bool SScreenShotBrowser::CanAddNewReportResult(const FImageComparisonResult& Comparison)
{
	return Comparison.IsNew();
}

#undef LOCTEXT_NAMESPACE
