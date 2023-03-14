// Copyright Epic Games, Inc. All Rights Reserved.
#include "RetargetEditor/IKRetargetOutputLogTabSummoner.h"

#include "IDocumentation.h"
#include "RigEditor/IKRigEditorStyle.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RigEditor/SIKRigOutputLog.h"
#include "Retargeter/IKRetargetProcessor.h"

#define LOCTEXT_NAMESPACE "IKRetargetOutputLogTabSummoner"

const FName FIKRetargetOutputLogTabSummoner::TabID(TEXT("RetargetOutputLog"));

FIKRetargetOutputLogTabSummoner::FIKRetargetOutputLogTabSummoner(
	const TSharedRef<FIKRetargetEditor>& InRetargetEditor)
	: FWorkflowTabFactory(TabID, InRetargetEditor),
	IKRetargetEditor(InRetargetEditor)
{
	bIsSingleton = true; // only allow a single instance of this tab
	
	TabLabel = LOCTEXT("IKRetargetOutputLogTabLabel", "Retarget Output Log");
	TabIcon = FSlateIcon(FIKRigEditorStyle::Get().GetStyleSetName(), "IKRig.TabIcon");

	ViewMenuDescription = LOCTEXT("IKRetargetOutputLog_ViewMenu_Desc", "Retarget Output Log");
	ViewMenuTooltip = LOCTEXT("IKRetargetOutputLog_ViewMenu_ToolTip", "Show the Retargeting Output Log Tab");
}

TSharedPtr<SToolTip> FIKRetargetOutputLogTabSummoner::CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const
{
	return  IDocumentation::Get()->CreateToolTip(LOCTEXT("IKRetargetOutputLogTooltip", "View warnings and errors while retargeting."), NULL, TEXT("Shared/Editors/Persona"), TEXT("IKRetargetOutputLog_Window"));
}

TSharedRef<SWidget> FIKRetargetOutputLogTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	const TSharedRef<FIKRetargetEditorController>& Controller = IKRetargetEditor.Pin()->GetController();
	const FName LogName = Controller->GetRetargetProcessor()->Log.GetLogTarget();
	TSharedRef<SIKRigOutputLog> LogView = SNew(SIKRigOutputLog, LogName);
	Controller->SetOutputLogView(LogView);
	return LogView;
}

#undef LOCTEXT_NAMESPACE 
