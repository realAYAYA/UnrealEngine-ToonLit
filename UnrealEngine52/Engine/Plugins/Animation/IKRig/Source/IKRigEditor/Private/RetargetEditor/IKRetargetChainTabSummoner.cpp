// Copyright Epic Games, Inc. All Rights Reserved.
#include "RetargetEditor/IKRetargetChainTabSummoner.h"

#include "IDocumentation.h"
#include "RigEditor/IKRigEditorStyle.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/SIKRetargetChainMapList.h"

#define LOCTEXT_NAMESPACE "RetargetChainTabSummoner"

const FName FIKRetargetChainTabSummoner::TabID(TEXT("ChainMapping"));

FIKRetargetChainTabSummoner::FIKRetargetChainTabSummoner(const TSharedRef<FIKRetargetEditor>& InRetargetEditor)
	: FWorkflowTabFactory(TabID, InRetargetEditor),
	IKRetargetEditor(InRetargetEditor)
{
	bIsSingleton = true; // only allow a single instance of this tab
	
	TabLabel = LOCTEXT("IKRetargetChainTabLabel", "Chain Mapping");
	TabIcon = FSlateIcon(FIKRigEditorStyle::Get().GetStyleSetName(), "IKRig.TabIcon");

	ViewMenuDescription = LOCTEXT("IKRetargetChain_ViewMenu_Desc", "Chain Mapping");
	ViewMenuTooltip = LOCTEXT("IKRetargetChain_ViewMenu_ToolTip", "Show the Chain Mapping Tab");
}

TSharedPtr<SToolTip> FIKRetargetChainTabSummoner::CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const
{
	return  IDocumentation::Get()->CreateToolTip(LOCTEXT("IKRetargetChainTooltip", "Map the bone chains between the source and target IK rigs."), NULL, TEXT("Shared/Editors/Persona"), TEXT("IKRetargetChain_Window"));
}

TSharedRef<SWidget> FIKRetargetChainTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SIKRetargetChainMapList, IKRetargetEditor.Pin()->GetController());
}

#undef LOCTEXT_NAMESPACE 
