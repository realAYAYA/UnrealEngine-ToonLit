// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubEditorModule.h"

#include "SLiveLinkHubEditorStatusBar.h"
#include "ToolMenus.h"

static TAutoConsoleVariable<int32> CVarLiveLinkHubEnableStatusBar(
	TEXT("LiveLinkHub.EnableStatusBar"), 1,
	TEXT("Whether to enable showing the livelink hub status bar in the editor. Must be set before launching the editor."),
	ECVF_RenderThreadSafe);

void FLiveLinkHubEditorModule::StartupModule()
{
	if (!IsRunningCommandlet() && CVarLiveLinkHubEnableStatusBar.GetValueOnAnyThread())
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FLiveLinkHubEditorModule::OnPostEngineInit);
	}
}

void FLiveLinkHubEditorModule::ShutdownModule()
{
	if (!IsRunningCommandlet() && CVarLiveLinkHubEnableStatusBar.GetValueOnAnyThread())
	{
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);
		UnregisterLiveLinkHubStatusBar();
	}
}

void FLiveLinkHubEditorModule::OnPostEngineInit()
{
	if (GEditor)
	{
		RegisterLiveLinkHubStatusBar();
	}
}

void FLiveLinkHubEditorModule::RegisterLiveLinkHubStatusBar()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.StatusBar.ToolBar"));

	FToolMenuSection& LiveLinkHubSection = Menu->AddSection(TEXT("LiveLinkHub"), FText::GetEmpty(), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

	LiveLinkHubSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("LiveLinkHubStatusBar"), CreateLiveLinkHubWidget(), FText::GetEmpty(), true, false)
	);
}

void FLiveLinkHubEditorModule::UnregisterLiveLinkHubStatusBar()
{
	UToolMenus::UnregisterOwner(this);
}

TSharedRef<SWidget> FLiveLinkHubEditorModule::CreateLiveLinkHubWidget()
{
	return SNew(SLiveLinkHubEditorStatusBar);
}

IMPLEMENT_MODULE(FLiveLinkHubEditorModule, LiveLinkHubEditor);
