// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigInfluenceMapTabSummoner.h"
#include "Editor/SRigHierarchy.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "SKismetInspector.h"
#include "ControlRigBlueprint.h"

#define LOCTEXT_NAMESPACE "RigInfluenceMapTabSummoner"

const FName FRigInfluenceMapTabSummoner::TabID(TEXT("RigInfluenceMap"));

FRigInfluenceMapTabSummoner::FRigInfluenceMapTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("RigInfluenceMapTabLabel", "Rig Influence Map");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RigInfluenceMap_ViewMenu_Desc", "Rig Influence Map");
	ViewMenuTooltip = LOCTEXT("RigInfluenceMap_ViewMenu_ToolTip", "Show the Rig Influence Map tab");
}

TSharedRef<SWidget> FRigInfluenceMapTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);

	if (ControlRigEditor.IsValid())
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(ControlRigEditor.Pin()->GetBlueprintObj()))
		{
			TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(
				FRigInfluenceMapPerEvent::StaticStruct(), (uint8*)&RigBlueprint->Influences));
			StructToDisplay->SetPackage(RigBlueprint->GetOutermost());
			KismetInspector->ShowSingleStruct(StructToDisplay);
		}
	}

	return KismetInspector;
}

#undef LOCTEXT_NAMESPACE 
