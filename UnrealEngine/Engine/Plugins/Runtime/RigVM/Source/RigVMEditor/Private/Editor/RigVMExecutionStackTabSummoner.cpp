// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMExecutionStackTabSummoner.h"
#include "Widgets/SRigVMExecutionStackView.h"
#include "Editor/RigVMEditor.h"

#define LOCTEXT_NAMESPACE "RigVMExecutionStackTabSummoner"

const FName FRigVMExecutionStackTabSummoner::TabID(TEXT("Execution Stack"));

FRigVMExecutionStackTabSummoner::FRigVMExecutionStackTabSummoner(const TSharedRef<FRigVMEditor>& InRigVMEditor)
	: FWorkflowTabFactory(TabID, InRigVMEditor)
	, RigVMEditor(InRigVMEditor)
{
	TabLabel = LOCTEXT("RigVMExecutionStackTabLabel", "Execution Stack");
	TabIcon = FSlateIcon(TEXT("RigVMEditorStyle"), "ExecutionStack.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RigVMExecutionStack_ViewMenu_Desc", "Execution Stack");
	ViewMenuTooltip = LOCTEXT("RigVMExecutionStack_ViewMenu_ToolTip", "Show the Execution Stack tab");
}

TSharedRef<SWidget> FRigVMExecutionStackTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SRigVMExecutionStackView, RigVMEditor.Pin().ToSharedRef());
}

#undef LOCTEXT_NAMESPACE 
