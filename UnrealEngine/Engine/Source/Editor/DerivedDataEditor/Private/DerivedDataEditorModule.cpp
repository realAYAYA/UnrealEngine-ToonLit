// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataEditorModule.h"
#include "DerivedDataInformation.h"
#include "SDerivedDataStatusBar.h"
#include "SDerivedDataDialogs.h"
#include "SDerivedDataCacheSettings.h"
#include "ZenServerInterface.h"
#include "DerivedDataCacheNotifications.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/AppStyle.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "DerivedDataEditor"

IMPLEMENT_MODULE(FDerivedDataEditorModule, DerivedDataEditor );

static const FName DerivedDataResourceUsageTabName = FName(TEXT("DerivedDataResourceUsage"));
static const FName DerivedDataCacheStatisticsTabName = FName(TEXT("DerivedDataCacheStatistics"));

void FDerivedDataEditorModule::StartupModule()
{
	const FSlateIcon ResourceUsageIcon(FAppStyle::GetAppStyleSetName(), "DerivedData.ResourceUsage");

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DerivedDataResourceUsageTabName, FOnSpawnTab::CreateRaw(this, &FDerivedDataEditorModule::CreateResourceUsageTab))
		.SetDisplayName(LOCTEXT("DerivedDataResourceUsageTabTitle", "Resource Usage"))
		.SetTooltipText(LOCTEXT("DerivedDataResourceUsageTabToolTipText", "Derived Data Resource Usage"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(ResourceUsageIcon);

	const FSlateIcon CacheStatisticsIcon(FAppStyle::GetAppStyleSetName(), "DerivedData.Cache.Statistics");

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DerivedDataCacheStatisticsTabName, FOnSpawnTab::CreateRaw(this, &FDerivedDataEditorModule::CreateCacheStatisticsTab))
		.SetDisplayName(LOCTEXT("DerivedDataCacheStatisticsTabTitle", "Cache Statistics"))
		.SetTooltipText(LOCTEXT("DerivedDataCacheStatisticsTabToolTipText", "Derived Data Cache Statistics"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(CacheStatisticsIcon);

#if WITH_RELOAD
	// This code attempts to relaunch the tabs when you reload this module
	if (IsReloadActive() && FSlateApplication::IsInitialized())
	{
		ShowCacheStatisticsTab();
		ShowResourceUsageTab();
	}
#endif // WITH_RELOAD

	FDerivedDataStatusBarMenuCommands::Register();

	DerivedDataCacheNotifications.Reset(new FDerivedDataCacheNotifications);
}

void FDerivedDataEditorModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DerivedDataResourceUsageTabName);

		if (ResourceUsageTab.IsValid())
		{
			ResourceUsageTab.Pin()->RequestCloseTab();
		}

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DerivedDataCacheStatisticsTabName);

		if (CacheStatisticsTab.IsValid())
		{
			CacheStatisticsTab.Pin()->RequestCloseTab();
		}
	}

	FDerivedDataStatusBarMenuCommands::Unregister();
}

TSharedRef<SWidget> FDerivedDataEditorModule::CreateStatusBarWidget()
{
	return SNew(SDerivedDataStatusBarWidget);
}

TSharedPtr<SWidget> FDerivedDataEditorModule::CreateResourceUsageDialog()
{
	return SNew(SDerivedDataResourceUsageDialog);
}

TSharedRef<SDockTab> FDerivedDataEditorModule::CreateResourceUsageTab(const FSpawnTabArgs& Args)
{
	return SAssignNew(ResourceUsageTab, SDockTab)
	.TabRole(ETabRole::NomadTab)
	[
		CreateResourceUsageDialog().ToSharedRef()
	];
}

void FDerivedDataEditorModule::ShowResourceUsageTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(DerivedDataResourceUsageTabName));
}


TSharedPtr<SWidget> FDerivedDataEditorModule::CreateCacheStatisticsDialog()
{
	return SNew(SDerivedDataCacheStatisticsDialog);
}


TSharedRef<SDockTab> FDerivedDataEditorModule::CreateCacheStatisticsTab(const FSpawnTabArgs& Args)
{
	return SAssignNew(CacheStatisticsTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateCacheStatisticsDialog().ToSharedRef()
		];
}

void FDerivedDataEditorModule::ShowCacheStatisticsTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(DerivedDataCacheStatisticsTabName));
}

void FDerivedDataEditorModule::ShowSettingsDialog()
{
	if (SettingsWindow.IsValid())
	{
		SettingsWindow->BringToFront();
	}
	else
	{
		// Create the window
		SettingsWindow = SNew(SWindow)
			.Title(LOCTEXT("DerrivedDataCacheSettingsWindowTitle", "Cache Settings"))
			.HasCloseButton(true)
			.SupportsMaximize(false)
			.SupportsMinimize(true)
			.SizingRule(ESizingRule::Autosized);

		// Set the closed callback
		SettingsWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this,&FDerivedDataEditorModule::OnSettingsDialogClosed));

		// Setup the content for the created window.
		SettingsWindow->SetContent(SAssignNew(SettingsDialog, SDerivedDataCacheSettingsDialog));

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if (RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(SettingsWindow.ToSharedRef(), RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(SettingsWindow.ToSharedRef());
		}
	}
}

void FDerivedDataEditorModule::OnSettingsDialogClosed(const TSharedRef<SWindow>& InWindow)
{
	SettingsWindow = nullptr;
	SettingsDialog = nullptr;
}

#undef LOCTEXT_NAMESPACE
