// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorTraceUtilities.h"

#include "ToolMenus.h"
#include "UnrealInsightsLauncher.h"

DEFINE_LOG_CATEGORY(LogTraceUtilities)

#define LOCTEXT_NAMESPACE "FEditorTraceUtilitiesModule"

void FEditorTraceUtilitiesModule::StartupModule()
{
	LLM_SCOPE_BYNAME(TEXT("Insights"));
	RegisterStartupCallbackHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateSP(
			FUnrealInsightsLauncher::Get().ToSharedRef(),
			&FUnrealInsightsLauncher::RegisterMenus));
}

void FEditorTraceUtilitiesModule::ShutdownModule()
{
	LLM_SCOPE_BYNAME(TEXT("Insights"));
	UToolMenus::UnRegisterStartupCallback(RegisterStartupCallbackHandle);
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FEditorTraceUtilitiesModule, EditorTraceUtilities)