// Copyright Epic Games, Inc. All Rights Reserved.

#include "TabFactory/DebugLogTabSummoner.h"

#include "Debug/SDebugLog.h"
#include "UMGStyle.h"
#include "WidgetBlueprintEditor.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName FDebugLogTabSummoner::TabID("DebugLog");

FDebugLogTabSummoner::FDebugLogTabSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor, FName InLogName)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
{
	LogName = InLogName;

	TabLabel = LOCTEXT("DebugLog_TabLabel", "Debug Log");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintDebugger.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DebugLog_ViewMenu_Desc", "Debug Log");
	ViewMenuTooltip = LOCTEXT("DebugLog_ViewMenu_ToolTip", "Show the Debug Log");
}

TSharedRef<SWidget> FDebugLogTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(UE::UMG::SDebugLog, LogName);
}

#undef LOCTEXT_NAMESPACE 
