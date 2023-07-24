// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraphEditorSchemaActions.h"
#include "AnimNextInterfaceEditorUtils.h"
#include "AnimNextInterfaceGraph_EdGraph.h"
#include "AnimNextInterfaceGraph_EdGraphNode.h"
#include "SGraphActionMenu.h"
#include "AnimNextInterfaceUncookedOnlyUtils.h"
#include "AnimNextInterfaceGraph_EditorData.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Settings/ControlRigSettings.h"

UEdGraphNode* FAnimNextInterfaceGraphSchemaAction_RigUnit::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UAnimNextInterfaceGraph_EditorData* EditorData = ParentGraph->GetTypedOuter<UAnimNextInterfaceGraph_EditorData>();
	UAnimNextInterfaceGraph_EdGraphNode* NewNode = nullptr;
	UAnimNextInterfaceGraph_EdGraph* EdGraph = Cast<UAnimNextInterfaceGraph_EdGraph>(ParentGraph);

	if (EditorData != nullptr && EdGraph != nullptr)
	{
		FName Name = UE::AnimNext::InterfaceGraphEditor::FUtils::ValidateName(EditorData, StructTemplate->GetFName().ToString());
		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		FRigVMUnitNodeCreatedContext& UnitNodeCreatedContext = Controller->GetUnitNodeCreatedContext();
		FRigVMUnitNodeCreatedContext::FScope ReasonScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::NodeSpawner);

		if (URigVMUnitNode* ModelNode = Controller->AddUnitNode(StructTemplate, FRigUnit::GetMethodName(), Location, Name.ToString(), true, false))
		{
			NewNode = Cast<UAnimNextInterfaceGraph_EdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);
			}

#if WITH_EDITORONLY_DATA

			// @TODO: switch over to custom settings here
			const FControlRigSettingsPerPinBool* ExpansionMapPtr = UControlRigEditorSettings::Get()->RigUnitPinExpansion.Find(ModelNode->GetScriptStruct()->GetName());
			if (ExpansionMapPtr)
			{
				const FControlRigSettingsPerPinBool& ExpansionMap = *ExpansionMapPtr;

				for (const TPair<FString, bool>& Pair : ExpansionMap.Values)
				{
					FString PinPath = FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *Pair.Key);
					Controller->SetPinExpansion(PinPath, Pair.Value, true);
				}
			}
		}
#endif
		
		Controller->CloseUndoBracket();
	}

	return NewNode;
}
