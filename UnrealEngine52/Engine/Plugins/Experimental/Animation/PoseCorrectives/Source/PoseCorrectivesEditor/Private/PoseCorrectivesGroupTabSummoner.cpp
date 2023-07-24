// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesGroupTabSummoner.h"
#include "PoseCorrectivesEditorToolkit.h"
#include "IDocumentation.h"
//#include "RigEditor/IKRigEditorStyle.h"
//#include "RigEditor/IKRigToolkit.h"
#include "SPoseCorrectivesGroups.h"

#define LOCTEXT_NAMESPACE "PoseCorrectivesGroupTabSummoner"

const FName FPoseCorrectivesGroupTabSummoner::TabID(TEXT("PoseCorrectivesGroups"));

FPoseCorrectivesGroupTabSummoner::FPoseCorrectivesGroupTabSummoner(const TSharedRef<FPoseCorrectivesEditorToolkit>& InPoseCorrectivesEditor)
	: FWorkflowTabFactory(TabID, InPoseCorrectivesEditor)
	, PoseCorrectivesEditor(InPoseCorrectivesEditor)
{
	bIsSingleton = true; // only allow a single instance of this tab
	
	TabLabel = LOCTEXT("PoseCorrectivesGroupTabLabel", "Groups");
	//TabIcon = FSlateIcon(FIKRigEditorStyle::Get().GetStyleSetName(), "IKRig.TabIcon");

	ViewMenuDescription = LOCTEXT("PoseCorrectivesGroup_ViewMenu_Desc", "Groups");
	ViewMenuTooltip = LOCTEXT("PoseCorrectivesGroup_ViewMenu_ToolTip", "Show the Pose Correctives Group Tab");
}

TSharedPtr<SToolTip> FPoseCorrectivesGroupTabSummoner::CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const
{
	return  IDocumentation::Get()->CreateToolTip(LOCTEXT("PoseCorrectivesGroupTooltip", "The Groups tab lets you manage the groups"), NULL, TEXT("Shared/Editors/Persona"), TEXT("PoseCorrectivesGroup_Window"));
}

TSharedRef<SWidget> FPoseCorrectivesGroupTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SPoseCorrectivesGroups, PoseCorrectivesEditor.Pin()->GetController()); //pass controller here..
}

#undef LOCTEXT_NAMESPACE 
