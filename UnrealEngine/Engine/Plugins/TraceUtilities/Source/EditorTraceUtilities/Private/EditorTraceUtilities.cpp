// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorTraceUtilities.h"

#include "Misc/ConfigContext.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogTraceUtilities)

#define LOCTEXT_NAMESPACE "FEditorTraceUtilitiesModule"

/**
  * This function will add the SInsightsStatusBarWidget to the Editor's status bar at the bottom ("LevelEditor.StatusBar.ToolBar").
  */
void RegisterInsightsStatusWidgetWithToolMenu();

FString FEditorTraceUtilitiesModule::EditorTraceUtilitiesIni;

void FEditorTraceUtilitiesModule::StartupModule()
{
	LLM_SCOPE_BYNAME(TEXT("Insights"));

	RegisterInsightsStatusWidgetWithToolMenu();

	FConfigContext::ReadIntoGConfig().Load(TEXT("TraceUtilities"), EditorTraceUtilitiesIni);
}

void FEditorTraceUtilitiesModule::ShutdownModule()
{
	LLM_SCOPE_BYNAME(TEXT("Insights"));
	UToolMenus::UnRegisterStartupCallback(RegisterStartupCallbackHandle);
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FEditorTraceUtilitiesModule, EditorTraceUtilities)