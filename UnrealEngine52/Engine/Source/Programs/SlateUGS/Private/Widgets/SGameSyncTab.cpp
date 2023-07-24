// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameSyncTab.h"
#include "SSyncFilterWindow.h"
#include "SSettingsWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "SPositiveActionButton.h"
#include "SSimpleComboButton.h"
#include "SSimpleButton.h"
#include "Widgets/SHordeBadge.h"
#include "Widgets/SScheduledSyncWindow.h"
#include "Widgets/SLogWidget.h"
#include "Widgets/Layout/SHeader.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Views/SListView.h"

#include "SlateUGSStyle.h"

#include "UGSTab.h"
#include "UGSTabManager.h"

#define LOCTEXT_NAMESPACE "UGSWindow"

namespace
{
	const FName HordeTableColumnStatus(TEXT("Status"));
	const FName HordeTableColumnType(TEXT("Type"));
	const FName HordeTableColumnChange(TEXT("Change"));
	const FName HordeTableColumnTime(TEXT("Time"));
	const FName HordeTableColumnAuthor(TEXT("Author"));
	const FName HordeTableColumnDescription(TEXT("Description"));
	const FName HordeTableColumnEditor(TEXT("Editor"));
}

void SBuildDataRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FChangeInfo>& Item)
{
	CurrentItem = Item;
	SMultiColumnTableRow<TSharedPtr<FChangeInfo>>::Construct(SMultiColumnTableRow::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SBuildDataRow::GenerateWidgetForColumn(const FName& ColumnId) // Todo: maybe can refactor some of this code so there's less duplication by using the root SWidget class on different types
{
	if (ColumnId == HordeTableColumnStatus)
	{
		TSharedRef<SImage> StatusCircle = SNew(SImage).Image(FSlateUGSStyle::Get().GetBrush("Icons.FilledCircle"));

		// Todo: switch to using CurrentItem->ReviewStatus when that variable actually gets filled
		UGSCore::EReviewVerdict Verdict = UGSCore::EReviewVerdict::Unknown;
		if (FMath::RandRange(0, 10) > 6)
		{
			Verdict = static_cast<UGSCore::EReviewVerdict>(FMath::RandRange(1, 3));
		}
		switch (Verdict)
		// switch (CurrentItem->ReviewStatus)
		{
			case UGSCore::EReviewVerdict::Good:
				StatusCircle->SetColorAndOpacity(FSlateUGSStyle::Get().GetColor("HordeBadge.Color.Success"));
				break;
			case UGSCore::EReviewVerdict::Bad:
				StatusCircle->SetColorAndOpacity(FSlateUGSStyle::Get().GetColor("HordeBadge.Color.Error"));
				break;
			case UGSCore::EReviewVerdict::Mixed:
				StatusCircle->SetColorAndOpacity(FSlateUGSStyle::Get().GetColor("HordeBadge.Color.Warning"));
				break;
			case UGSCore::EReviewVerdict::Unknown:
			default: // Fall through
				StatusCircle->SetColorAndOpacity(FSlateUGSStyle::Get().GetColor("HordeBadge.Color.Unknown"));
				break;
		}

		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				StatusCircle
			];
	}

	if (ColumnId == HordeTableColumnType)
	{
		bool bIsCode    = CurrentItem->ChangeType.bContainsCode;
		bool bIsContent = CurrentItem->ChangeType.bContainsContent;

		TSharedRef<SHorizontalBox> TypeBadges = SNew(SHorizontalBox);

		if (bIsCode)
		{
			TypeBadges->AddSlot()
			.Padding(1.0f, 0.0f)
			.HAlign(HAlign_Center)
			[
				SNew(SHordeBadge)
				.Text(FText::FromString("Code"))
				.BadgeState(EBadgeState::Pending)
			];
		}

		if (bIsContent)
		{
			TypeBadges->AddSlot()
			.Padding(1.0f, 0.0f)
			.HAlign(HAlign_Center)
			[
				SNew(SHordeBadge)
				.Text(FText::FromString("Content"))
				.BadgeState(EBadgeState::Pending)
			];
		}

		return TypeBadges;
	}

	TSharedRef<STextBlock> TextItem = SNew(STextBlock);
	if (ColumnId == HordeTableColumnChange)
	{
		TextItem->SetText(FText::FromString(FString::FromInt(CurrentItem->Changelist)));
	}
	if (ColumnId == HordeTableColumnTime)
	{
		TextItem->SetText(FText::FromString(CurrentItem->Time.ToString(TEXT("%h:%M %A"))));
	}
	if (ColumnId == HordeTableColumnAuthor)
	{
		TextItem->SetText(CurrentItem->Author);
	}
	if (ColumnId == HordeTableColumnDescription)
	{
		// Perforce descriptions mix line endings, so remove all possible line endings
		FString Description = CurrentItem->Description
			.Replace(TEXT("\r"), TEXT(" "))
			.Replace(TEXT("\n"), TEXT(" ")); // Todo: replacing both with spaces causes lots of double spaces, maybe filter that out?
		TextItem->SetText(FText::FromString(Description));
	}
	if (ColumnId == HordeTableColumnEditor)
	{
		// Todo: replace this dummy badge data with the real thing
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(SHordeBadge)
				.Text(FText::FromString("Editor Win64"))
				.BadgeState(static_cast<EBadgeState>(FMath::RandRange(0, 4)))
			]
			+SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			[
				SNew(SHordeBadge)
				.Text(FText::FromString("Editor Mac"))
				.BadgeState(static_cast<EBadgeState>(FMath::RandRange(0, 4)))
			]
			+SHorizontalBox::Slot()
			[
				SNew(SHordeBadge)
				.Text(FText::FromString("Editor Linux"))
				.BadgeState(static_cast<EBadgeState>(FMath::RandRange(0, 4)))
			];
	}

	if (CurrentItem->bSyncingPrecompiled)
	{
		if (!CurrentItem->bHasZippedBinaries)
		{
			TextItem->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f));
		}
	}

	if (CurrentItem->bCurrentlySynced)
	{
		// Lets make the font white and bold when we are the currently synced CL
		FSlateFontInfo FontInfo = FSlateUGSStyle::Get().GetFontStyle("NormalFontBold");

		TextItem->SetFont(FontInfo);
		TextItem->SetColorAndOpacity(FLinearColor::White);
	}

	return SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 2.5f)
		[
			TextItem
		];
}

TSharedRef<ITableRow> SGameSyncTab::GenerateHordeBuildTableRow(TSharedPtr<FChangeInfo> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	if (InItem->bHeaderRow)
	{
		return SNew(STableRow<TSharedPtr<FChangeInfo>>, InOwnerTable)
		.ShowSelection(false)
		[
			SNew(SBox)
			.Padding(5.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FSlateUGSStyle::Get().GetFontStyle("Font.Large.Bold"))
					.ColorAndOpacity(FLinearColor::White)
					.Text(FText::FromString(InItem->Time.ToFormattedString(TEXT("%A, %B %e, %Y"))))
				]
				+SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SSeparator)
					.SeparatorImage(FSlateUGSStyle::Get().GetBrush("Header.Post"))
				]
			]
		];
	}

	return SNew(SBuildDataRow, InOwnerTable, InItem).ToolTipText(FText::FromString(InItem->Description));
}

// Button callbacks
TSharedRef<SWidget> SGameSyncTab::MakeSyncButtonDropdown()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Sync Latest")),
		FText::FromString(TEXT("Sync to the latest submitted changelist")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(Tab, &UGSTab::OnSyncLatest)),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Setup Schedule Sync")),
		FText::FromString(TEXT("Setup a schedule sync to run at specific time")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this] { FSlateApplication::Get().AddModalWindow(SNew(SScheduledSyncWindow, Tab), Tab->GetTabArgs().GetOwnerWindow(), false); } )),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	return MenuBuilder.MakeWidget();
}

void SGameSyncTab::Construct(const FArguments& InArgs, UGSTab* InTab)
{
	Tab = InTab;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		// Toolbar at the top of the tab // Todo: Maybe use a FToolBarBuilder instead
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		[
			SNew(SBorder)
			.BorderImage(FSlateUGSStyle::Get().GetBrush("Brushes.Panel"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Refresh"))
						.ToolTipText(LOCTEXT("RefreshBuildList", "Refreshes the Build List"))
						.OnClicked_Lambda([this] { Tab->RefreshBuildList(); return FReply::Handled(); })
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleComboButton)
						.Text(LOCTEXT("Sync", "Sync"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.CircleArrowDown"))
						.HasDownArrow(true)
						.MenuContent()
						[
							MakeSyncButtonDropdown()
						]
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("CancelSync", "Cancel"))
						.ToolTipText(LOCTEXT("CancelSync", "Cancels the current sync/build operation"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.X"))
						.OnClicked_Lambda([this] { Tab->CancelSync(); return FReply::Handled(); })
						.IsEnabled_Lambda([this] { return Tab->IsSyncing(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SPositiveActionButton)
						.Text(LOCTEXT("NewProjectButton", "New Project")) // Todo: replace with new tab button eventually
						.OnClicked_Lambda([this] { Tab->GetTabManager()->ActivateTab(); return FReply::Handled(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("Build", "Build"))
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
						.OnClicked_Lambda([this] { Tab->OnBuildWorkspace(); return FReply::Handled(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("RunUnrealEditor", "Run Unreal Editor"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Launch"))
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
						.OnClicked_Lambda([this] { Tab->OnOpenEditor(); return FReply::Handled(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("OpenExplorer", "Open Explorer"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.FolderOpen")) // Todo: shouldn't use this icon (repurposing, also could use other IDEs)
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
						.OnClicked_Lambda([this] { Tab->OnOpenExplorer(); return FReply::Handled(); })
					]
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("BuildHealth", "Build Health")) // Todo: What icon?
						.IsEnabled(false) // Todo: enable after adding this functionality
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("SDKInfo", "SDK Info"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Settings")) // Todo: What icon?
						.IsEnabled(false) // Todo: enable after adding this functionality
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("OpenPerforce", "Open Perforce"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Blueprints")) // Todo: shouldn't use this icon (repurposing)
						.OnClicked_Lambda([this] { Tab->OnOpenPerforceClicked(); return FReply::Handled(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("CleanSolution", "Clean Solution"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Delete")) // Todo: shouldn't use this icon (repurposing)
						.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
						.IsEnabled(false) // Todo: enable after adding this functionality
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("Filter", "Filter"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Filter"))
						// Todo: this is probably the wrong "Filter" button. The functionality below should probably be in the settings dropdown
						.OnClicked_Lambda([this] { FSlateApplication::Get().AddModalWindow(SNew(SSyncFilterWindow, Tab), Tab->GetTabArgs().GetOwnerWindow(), false); return FReply::Handled(); })
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						SNew(SSimpleButton)
						.Text(LOCTEXT("Settings", "Settings"))
						.Icon(FSlateUGSStyle::Get().GetBrush("Icons.Settings"))
						.OnClicked_Lambda([this] { FSlateApplication::Get().AddModalWindow(SNew(SSettingsWindow, Tab), Tab->GetTabArgs().GetOwnerWindow(), false); return FReply::Handled(); })
					]
				]
			]
		]
		// Stream banner
		+SVerticalBox::Slot()
		.Padding(0.0f, 5.0f)
		.AutoHeight()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SSimpleGradient) // Todo: save literals in class and use different colors depending on stream (Fortnite, UE5, etc)
				.StartColor(FLinearColor(161.0f / 255.0f, 57.0f / 255.0f, 191.0f / 255.0f))
				.EndColor(FLinearColor(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f))
			]
			+SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				// Stream logo
				+SHorizontalBox::Slot()
				.Padding(35.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FSlateUGSStyle::Get().GetBrush("UnrealCircle.Thin"))
					.DesiredSizeOverride(CoreStyleConstants::Icon128x128)
				]
				// Stream, CL, and uproject path
				+SHorizontalBox::Slot()
				.Padding(0.0f, 35.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					// Labels
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					.Padding(10.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("StreamText", "STREAM"))
							.Font(FSlateUGSStyle::Get().GetFontStyle("NormalFontBold"))
							.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Right)
						.Padding(0.0f, 35.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ChangelistText", "CHANGELIST"))
							.Font(FSlateUGSStyle::Get().GetFontStyle("NormalFontBold"))
							.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ProjectText", "PROJECT"))
							.Font(FSlateUGSStyle::Get().GetFontStyle("NormalFontBold"))
							.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
						]
					]
					// Data
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SAssignNew(StreamPathText, STextBlock)
							.Text(LOCTEXT("StreamTextValue", "No stream path found"))
							.ColorAndOpacity(FLinearColor::White)
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						.Padding(0.0f, 35.0f)
						[
							SAssignNew(ChangelistText, STextBlock)
							.Text(LOCTEXT("ChangelistTextValue", "No changelist found"))
							.ColorAndOpacity(FLinearColor::White)
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SAssignNew(ProjectPathText, STextBlock)
							.Text(LOCTEXT("ProjectValue", "No project path found"))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
				// Syncing files progress
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox) // Todo: Only display this widget when syncing
					.Visibility_Lambda([this] { return Tab->IsSyncing() ? EVisibility::Visible : EVisibility::Hidden; })
					+SHorizontalBox::Slot()
					.Padding(5.0f, 25.0f)
					.AutoWidth()
					[
						SAssignNew(SyncProgressText, STextBlock)
						.Text(LOCTEXT("SyncProgress", "Syncing Files"))
						.Text_Lambda([this] { return FText::FromString(Tab->GetSyncProgress()); })
					]
					+SHorizontalBox::Slot()
					.Padding(0.0f, 12.5f, 12.5f, 5.0f)
					.AutoWidth()
					[
						SNew(SThrobber)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 5.0f)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			// Horde builds
			+SSplitter::Slot()
			.Value(0.65f)
			[
				SAssignNew(HordeBuildsView, SListView<TSharedPtr<FChangeInfo>>)
				.ListItemsSource(&HordeBuilds)
				.SelectionMode(ESelectionMode::Single)
				.IsEnabled_Lambda([this] { return !Tab->IsSyncing(); })
				.OnGenerateRow(this, &SGameSyncTab::GenerateHordeBuildTableRow)
				.OnContextMenuOpening(this, &SGameSyncTab::OnRightClickedBuild)
				.HeaderRow(
					SNew(SHeaderRow)
					+SHeaderRow::Column(HordeTableColumnStatus)
					.DefaultLabel(LOCTEXT("HordeHeaderStatus", ""))
					.FixedWidth(35.0f)
					+SHeaderRow::Column(HordeTableColumnType)
					.DefaultLabel(LOCTEXT("HordeHeaderType", "Type"))
					.FixedWidth(150.0f)
					+SHeaderRow::Column(HordeTableColumnChange)
					.DefaultLabel(LOCTEXT("HordeHeaderChange", "Change"))
					.FixedWidth(75.0f)
					+SHeaderRow::Column(HordeTableColumnTime)
					.DefaultLabel(LOCTEXT("HordeHeaderTime", "Time"))
					.FixedWidth(70.0f)
					+SHeaderRow::Column(HordeTableColumnAuthor)
					.DefaultLabel(LOCTEXT("HordeHeaderAuthor", "Author"))
					.FillWidth(0.15f)
					+SHeaderRow::Column(HordeTableColumnDescription)
					.DefaultLabel(LOCTEXT("HordeHeaderDescription", "Description"))
					+SHeaderRow::Column(HordeTableColumnEditor)
					.DefaultLabel(LOCTEXT("HordeHeaderEditor", "Editor"))
					.FillWidth(0.5f)
				)
			]
			// Log
			+SSplitter::Slot()
			.Value(0.35f)
			[
				SAssignNew(SyncLog, SLogWidget)
			]
		]
	];
}

TSharedPtr<SLogWidget> SGameSyncTab::GetSyncLog() const
{
	return SyncLog;
}

bool SGameSyncTab::SetSyncLogLocation(const FString& LogFileName)
{
    return SyncLog->OpenFile(*LogFileName);
}

void SGameSyncTab::SetStreamPathText(FText StreamPath)
{
	StreamPathText->SetText(StreamPath);
}

void SGameSyncTab::SetChangelistText(int Changelist)
{
	if (Changelist > 0)
	{
		ChangelistText->SetText(FText::FromString(FString::FromInt(Changelist)));
	}
	else
	{
		ChangelistText->SetText(FText::FromString(TEXT("Unknown")));
	}
}

void SGameSyncTab::SetProjectPathText(FText ProjectPath)
{
	ProjectPathText->SetText(ProjectPath);
}

void SGameSyncTab::AddHordeBuilds(const TArray<TSharedPtr<FChangeInfo>>& Builds)
{
	HordeBuilds = Builds;
	HordeBuildsView->RebuildList();
}

TSharedPtr<SWidget> SGameSyncTab::OnRightClickedBuild()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	TArray<TSharedPtr<FChangeInfo>> SelectedItems = HordeBuildsView->GetSelectedItems(); // Todo: since I disabled multi select, I might be able to assume the array size is always 1?
	if (SelectedItems.IsValidIndex(0))
	{
		// Don't show menu items for header rows; OnContextMenuOpening handles nullptr by not showing a menu
		if (SelectedItems[0]->bHeaderRow)
		{
			return nullptr;
		}

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Sync")),
			FText::FromString("Sync to CL " + FString::FromInt(SelectedItems[0]->Changelist)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(Tab, &UGSTab::OnSyncChangelist, SelectedItems[0]->Changelist)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSeparator();

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("View in Swarm...")),
			TAttribute<FText>(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(Tab, &UGSTab::OnViewInSwarmClicked, SelectedItems[0]->Changelist)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Copy Changelist")),
			TAttribute<FText>(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(Tab, &UGSTab::OnCopyChangeListClicked, SelectedItems[0]->Changelist)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("More Info")),
			TAttribute<FText>(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(Tab, &UGSTab::OnMoreInfoClicked, SelectedItems[0]->Changelist)),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
