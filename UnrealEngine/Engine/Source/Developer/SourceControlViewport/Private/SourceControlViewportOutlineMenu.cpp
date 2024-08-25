// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlViewportOutlineMenu.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "HAL/IConsoleManager.h"
#include "EngineAnalytics.h"
#include "LevelEditorMenuContext.h"
#include "LevelEditorViewport.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "SourceControlViewportOutlineMenu"

static bool bEnableViewportOutlineMenu = false;
TAutoConsoleVariable<bool> CVarSourceControlEnableViewportOutlineMenu(
	TEXT("SourceControl.ViewportOutlineMenu.Enable"),
	bEnableViewportOutlineMenu,
	TEXT("Enables an options menu that allows to toggle source control outlines on or off."),
	ECVF_Default);

static const FName MenuName("LevelEditor.LevelViewportToolbar.Show");
static const FName SectionName("RevisionControl");
static const FName SubMenuName("ShowRevisionControlMenu");

FSourceControlViewportOutlineMenu::FSourceControlViewportOutlineMenu()
{
}

FSourceControlViewportOutlineMenu::~FSourceControlViewportOutlineMenu()
{
	RemoveViewportOutlineMenu();
}

void FSourceControlViewportOutlineMenu::Init()
{
	CVarSourceControlEnableViewportOutlineMenu->AsVariable()->OnChangedDelegate().AddSPLambda(this,
		[this](IConsoleVariable* EnableViewportOutlineMenu)
		{
			if (EnableViewportOutlineMenu->GetBool())
			{
				InsertViewportOutlineMenu();
			}
			else
			{
				RemoveViewportOutlineMenu();
			}
		}
	);
}

void FSourceControlViewportOutlineMenu::InsertViewportOutlineMenu()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		UToolMenu* Menu = ToolMenus->ExtendMenu(MenuName);
		if (Menu != nullptr)
		{
			Menu->AddDynamicSection(SectionName,
				FNewToolMenuDelegate::CreateSP(this, &FSourceControlViewportOutlineMenu::PopulateViewportOutlineMenu)
			);
		}
	}
}

void FSourceControlViewportOutlineMenu::PopulateViewportOutlineMenu(UToolMenu* InMenu)
{
	check(InMenu);

	InMenu->AddDynamicSection(SectionName, FNewToolMenuDelegate::CreateLambda(
		[this](UToolMenu* InMenu)
		{
			ULevelViewportToolBarContext* Context = InMenu->FindContext<ULevelViewportToolBarContext>();
			if (!Context)
			{
				return;
			}

			FLevelEditorViewportClient* ViewportClient = Context->GetLevelViewportClient();
			if (!ViewportClient || !ViewportClient->IsPerspective())
			{
				return;
			}

			FToolMenuSection& RevisionControlSection = InMenu->FindOrAddSection(SectionName, LOCTEXT("RevisionControl", "Revision Control"));
			RevisionControlSection.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda(
				[this, ViewportClient](FToolMenuSection& InSection)
				{
					InSection.AddSubMenu(
						SubMenuName,
						LOCTEXT("RevisionControlSubMenu", "Status Highlighting"),
						LOCTEXT("RevisionControlSubMenu_ToolTip", "Toggle revision control status highlights in the viewport on or off."),
						FNewToolMenuDelegate::CreateLambda(
							[this, ViewportClient](UToolMenu* InSubMenu)
							{
								FToolMenuSection& DefaultSection = InSubMenu->AddSection(NAME_None);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("ShowAll", "Show All"),
									LOCTEXT("ShowAll_ToolTip", "Enable highlighting for all statuses"),
									FSlateIcon(),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::ShowAll, ViewportClient)
									),
									EUserInterfaceActionType::Button
								);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("HideAll", "Hide All"),
									LOCTEXT("HideAll_ToolTip", "Disable highlighting for all statuses"),
									FSlateIcon(),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::HideAll, ViewportClient)
									),
									EUserInterfaceActionType::Button
								);

								DefaultSection.AddSeparator(NAME_None);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("HighlightCheckedOutByOtherUser", "Checked Out by Others"),
									LOCTEXT("HighlightCheckedOutByOtherUser_ToolTip", "Highlight objects that are checked out by someone else."),
									FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOutByOtherUser"),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::ToggleHighlight, ViewportClient, ESourceControlStatus::CheckedOutByOtherUser),
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &FSourceControlViewportOutlineMenu::IsHighlighted, ViewportClient, ESourceControlStatus::CheckedOutByOtherUser)
									),
									EUserInterfaceActionType::ToggleButton
								);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("HighlightNotAtHeadRevision", "Out of Date"),
									LOCTEXT("HighlightNotAtHeadRevision_ToolTip", "Highlight objects that are not at the latest revision."),
									FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotAtHeadRevision"),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::ToggleHighlight, ViewportClient, ESourceControlStatus::NotAtHeadRevision),
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &FSourceControlViewportOutlineMenu::IsHighlighted, ViewportClient, ESourceControlStatus::NotAtHeadRevision)
									),
									EUserInterfaceActionType::ToggleButton
								);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("HighlightCheckedOut", "Checked Out by Me"),
									LOCTEXT("HighlightCheckedOut_ToolTip", "Highlight objects that are checked out by me."),
									FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut"),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::ToggleHighlight, ViewportClient, ESourceControlStatus::CheckedOut),
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &FSourceControlViewportOutlineMenu::IsHighlighted, ViewportClient, ESourceControlStatus::CheckedOut)
									),
									EUserInterfaceActionType::ToggleButton
								);

								DefaultSection.AddMenuEntry(
									NAME_None,
									LOCTEXT("HighlightOpenForAdd", "Newly Added"),
									LOCTEXT("HighlightOpenForAdd_ToolTip", "Highlight objects that have been added by me."),
									FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.OpenForAdd"),
									FUIAction(
										FExecuteAction::CreateSP(this, &FSourceControlViewportOutlineMenu::ToggleHighlight, ViewportClient, ESourceControlStatus::OpenForAdd),
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &FSourceControlViewportOutlineMenu::IsHighlighted, ViewportClient, ESourceControlStatus::OpenForAdd)
									),
									EUserInterfaceActionType::ToggleButton
								);
							}
						),
						false,
						FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.RevisionControl")
					);
				}));
		}
	)
	);
}

void FSourceControlViewportOutlineMenu::RemoveViewportOutlineMenu()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		UToolMenu* Menu = ToolMenus->ExtendMenu(MenuName);
		if (Menu != nullptr)
		{
			Menu->RemoveSection(SectionName);
		}
	}
}

void FSourceControlViewportOutlineMenu::ShowAll(FLevelEditorViewportClient* ViewportClient)
{
	ensure(ViewportClient);

	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::CheckedOutByOtherUser, /*bEnabled=*/true);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::NotAtHeadRevision, /*bEnabled=*/true);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::CheckedOut, /*bEnabled=*/true);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::OpenForAdd, /*bEnabled=*/true);
	
	RecordToggleEvent(TEXT("All"), /*bEnabled=*/true);
}

void FSourceControlViewportOutlineMenu::HideAll(FLevelEditorViewportClient* ViewportClient)
{
	ensure(ViewportClient);

	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::CheckedOutByOtherUser, /*bEnabled=*/false);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::NotAtHeadRevision, /*bEnabled=*/false);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::CheckedOut, /*bEnabled=*/false);
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, ESourceControlStatus::OpenForAdd, /*bEnabled=*/false);

	RecordToggleEvent(TEXT("All"), /*bEnabled=*/false);
}

void FSourceControlViewportOutlineMenu::ToggleHighlight(FLevelEditorViewportClient* ViewportClient, ESourceControlStatus Status)
{
	ensure(ViewportClient);

	bool bOld = SourceControlViewportUtils::GetFeedbackEnabled(ViewportClient, Status);
	bool bNew = bOld ? false : true;
	SourceControlViewportUtils::SetFeedbackEnabled(ViewportClient, Status, bNew);

	FString EnumValueWithoutType = UEnum::GetValueAsString(Status)
		.Replace(TEXT("ESourceControlStatus::"), TEXT(""));
	RecordToggleEvent(EnumValueWithoutType, bNew);
}

bool FSourceControlViewportOutlineMenu::IsHighlighted(FLevelEditorViewportClient* ViewportClient, ESourceControlStatus Status) const
{
	ensure(ViewportClient);

	return SourceControlViewportUtils::GetFeedbackEnabled(ViewportClient, Status);
}

void FSourceControlViewportOutlineMenu::RecordToggleEvent(const FString& Param, bool bEnabled) const
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(
			TEXT("Editor.Usage.SourceControl.OutlineSettings"), Param, bEnabled ? TEXT("True") : TEXT("False")
		);
	}
}

#undef LOCTEXT_NAMESPACE