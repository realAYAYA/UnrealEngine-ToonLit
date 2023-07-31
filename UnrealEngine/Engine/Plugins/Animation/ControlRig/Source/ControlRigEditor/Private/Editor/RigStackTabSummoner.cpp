// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigStackTabSummoner.h"
#include "Editor/SControlRigStackView.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"

#define LOCTEXT_NAMESPACE "RigStackTabSummoner"

const FName FRigStackTabSummoner::TabID(TEXT("Execution Stack"));

FRigStackTabSummoner::FRigStackTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("RigStackTabLabel", "Execution Stack");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ExecutionStack.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RigStack_ViewMenu_Desc", "Execution Stack");
	ViewMenuTooltip = LOCTEXT("RigStack_ViewMenu_ToolTip", "Show the Execution Stack tab");
}

TSharedRef<SWidget> FRigStackTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SControlRigStackView, ControlRigEditor.Pin().ToSharedRef());
}

#undef LOCTEXT_NAMESPACE 
