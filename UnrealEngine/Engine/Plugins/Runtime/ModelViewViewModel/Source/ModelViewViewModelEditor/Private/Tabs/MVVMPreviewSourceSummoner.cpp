// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tabs/MVVMPreviewSourceSummoner.h"

#include "Styling/MVVMEditorStyle.h"
#include "WidgetBlueprintEditor.h"
#include "Widgets/SMVVMPreviewSourcePanel.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "DebugSourcePanel"

namespace UE::MVVM
{

const FLazyName DebugSourcePanelTabID = "PreviewSourcePanel";

FName FPreviewSourceSummoner::GetTabID()
{
	return DebugSourcePanelTabID.Resolve();
}


FPreviewSourceSummoner::FPreviewSourceSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: FWorkflowTabFactory(GetTabID(), InBlueprintEditor)
	, WeakEditor(InBlueprintEditor)
{
	TabLabel = LOCTEXT("ViewmodelTabLabel", "Viewmodels");
	TabIcon = FSlateIcon(FMVVMEditorStyle::Get().GetStyleSetName(), "BlueprintView.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("Viewmodel_Desc", "Viewmodels");
	ViewMenuTooltip = LOCTEXT("Viewmodel_ToolTip", "Show the viewmodels panel");
}


TSharedRef<SWidget> FPreviewSourceSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	if (TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPtr = WeakEditor.Pin())
	{
		return SNew(SPreviewSourcePanel, BlueprintEditorPtr)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PreviewSourcePanel")));
	}
	return SNullWidget::NullWidget;
}

} // namespace

#undef LOCTEXT_NAMESPACE 
