// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNaniteTools.h"
#include "NaniteToolsModule.h"
#include "SNaniteAudit.h"
#include "NaniteAuditRegistry.h"
#include "NaniteToolsArguments.h"
#include "NaniteToolCommands.h"
#include "DetailsViewArgs.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "SPrimaryButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NaniteTools"

const FName FNaniteAuditTabs::ErrorsViewID(TEXT("Errors"));
const FName FNaniteAuditTabs::OptimizeViewID(TEXT("Optimize"));

void SNaniteTools::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	// Create & initialize tab manager.
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);

	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("NaniteToolsMenuGroupName", "Nanite Tools"));

	TabManager->RegisterTabSpawner(FNaniteAuditTabs::ErrorsViewID, FOnSpawnTab::CreateRaw(this, &SNaniteTools::SpawnTab_ErrorsView))
		.SetDisplayName(LOCTEXT("ErrorsViewTabTitle", "Errors"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Error"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(FNaniteAuditTabs::OptimizeViewID, FOnSpawnTab::CreateRaw(this, &SNaniteTools::SpawnTab_OptimizeView))
		.SetDisplayName(LOCTEXT("OptimizeViewTabTitle", "Optimize"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"))
		.SetGroup(AppMenuGroup);

	FNaniteToolCommands::Register();

	TabLayout = FTabManager::NewLayout("Nanite_Tools_Layout_v0.0.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(FNaniteAuditTabs::ErrorsViewID, ETabState::OpenedTab)
				->AddTab(FNaniteAuditTabs::OptimizeViewID, ETabState::OpenedTab)
				->SetForegroundTab(FNaniteAuditTabs::ErrorsViewID)
			)
		);

	ChildSlot
	[
		SNew(SOverlay)

		// Overlay slot for the main window area
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(10.0f)
			[
				SNew(SPrimaryButton)
				.Icon(FAppStyle::GetBrush("Icons.Refresh"))
				.Text(LOCTEXT("PerformAuditLoc", "Perform Audit"))
				.ToolTipText(LOCTEXT("PerformAuditTooltipLoc", "Performs an audit of all static meshes loaded in memory."))
				.OnClicked(FOnClicked::CreateSP(this, &SNaniteTools::OnPerformAudit))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				TabManager->RestoreFrom(TabLayout.ToSharedRef(), ConstructUnderWindow).ToSharedRef()
			]
		]
	];

	Audit();
}

SNaniteTools::~SNaniteTools()
{
}

FReply SNaniteTools::OnPerformAudit()
{
	Audit();
	HighlightImportantTab();
	return FReply::Handled();
}

void SNaniteTools::Audit()
{
	if (ErrorsView)
	{
		ErrorsView->PreAudit();
	}

	if (OptimizeView)
	{
		OptimizeView->PreAudit();
	}

	AuditRegistry = MakeShared<FNaniteAuditRegistry>();
	AuditRegistry->PerformAudit();

	if (ErrorsView)
	{
		ErrorsView->PostAudit(AuditRegistry);
	}

	if (OptimizeView)
	{
		OptimizeView->PostAudit(AuditRegistry);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SDockTab> SNaniteTools::SpawnTab_ErrorsView(const FSpawnTabArgs& Args)
{
	ErrorsTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(ErrorsView, SNaniteAudit, SNaniteAudit::AuditMode::Errors, this)
		];

	const TSharedRef<SDockTab> ErrorsTabRef = ErrorsTab.ToSharedRef();

	ErrorsTabRef->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SNaniteTools::OnErrorsViewTabClosed));
	ErrorsTabRef->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([ErrorsTabRef]()
	{
		// Prevent the user from closing this tab.
		return false;
	}));

	return ErrorsTabRef;
}

TSharedRef<SDockTab> SNaniteTools::SpawnTab_OptimizeView(const FSpawnTabArgs& Args)
{
	OptimizeTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(OptimizeView, SNaniteAudit, SNaniteAudit::AuditMode::Optimize, this)
		];

	const TSharedRef<SDockTab> OptimizeTabRef = OptimizeTab.ToSharedRef();

	OptimizeTabRef->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SNaniteTools::OnOptimizeViewTabClosed));
	OptimizeTabRef->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([OptimizeTabRef]()
	{
		// Prevent the user from closing this tab.
		return false;
	}));

	return OptimizeTabRef;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SNaniteTools::OnErrorsViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	if (ErrorsView)
	{
		ErrorsView = nullptr;
	}
}

void SNaniteTools::OnOptimizeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	if (OptimizeView)
	{
		OptimizeView = nullptr;
	}
}

void SNaniteTools::HighlightImportantTab()
{
	if (ErrorsTab && ErrorsView && ErrorsView->GetRowCount() > 0)
	{
		// Always highlight error tab if any are present
		TabManager->DrawAttention(ErrorsTab.ToSharedRef());
	}
	else if (OptimizeTab && OptimizeView && OptimizeView->GetRowCount() > 0)
	{
		// No errors, and some optimize records exist, highlight the tab
		TabManager->DrawAttention(OptimizeTab.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
