// Copyright Epic Games, Inc. All Rights Reserved.

#include "TabFactory/PreviewTabSummoner.h"

#include "Debug/SDebugPreview.h"
#include "UMGStyle.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName FPreviewTabSummoner::TabID("WidgetPreview");

FPreviewTabSummoner::FPreviewTabSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
{
	TabLabel = LOCTEXT("DebugPreview_TabLabel", "Preview");
	TabIcon = FSlateIcon(FUMGStyle::GetStyleSetName(), "BlueprintDebugger.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DebugPreview_ViewMenu_Desc", "Preview");
	ViewMenuTooltip = LOCTEXT("DebugPreview_ViewMenu_ToolTip", "Show the Preview");
}

TSharedRef<SWidget> FPreviewTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FWidgetBlueprintEditor>(BlueprintEditor.Pin());

	return SNew(UE::UMG::SDebugPreview, BlueprintEditorPtr)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Preview")));
}

#undef LOCTEXT_NAMESPACE 
