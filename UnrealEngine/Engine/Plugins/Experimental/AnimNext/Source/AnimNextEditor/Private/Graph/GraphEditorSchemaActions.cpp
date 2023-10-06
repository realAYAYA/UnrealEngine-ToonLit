// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphEditorSchemaActions.h"
#include "EditorUtils.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/AnimNextGraph_EdGraphNode.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Settings/ControlRigSettings.h"

UEdGraphNode* FAnimNextGraphSchemaAction_RigUnit::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UAnimNextGraph_EditorData* EditorData = ParentGraph->GetTypedOuter<UAnimNextGraph_EditorData>();
	UAnimNextGraph_EdGraphNode* NewNode = nullptr;
	UAnimNextGraph_EdGraph* EdGraph = Cast<UAnimNextGraph_EdGraph>(ParentGraph);

	if (EditorData != nullptr && EdGraph != nullptr)
	{
		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(EditorData, StructTemplate->GetFName().ToString());
		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		FRigVMUnitNodeCreatedContext& UnitNodeCreatedContext = Controller->GetUnitNodeCreatedContext();
		FRigVMUnitNodeCreatedContext::FScope ReasonScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::NodeSpawner);

		if (URigVMUnitNode* ModelNode = Controller->AddUnitNode(StructTemplate, FRigUnit::GetMethodName(), Location, Name.ToString(), true, false))
		{
			NewNode = Cast<UAnimNextGraph_EdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
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


UEdGraphNode* FAnimNextGraphSchemaAction_DispatchFactory::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UAnimNextGraph_EditorData* EditorData = ParentGraph->GetTypedOuter<UAnimNextGraph_EditorData>();
	UAnimNextGraph_EdGraphNode* NewNode = nullptr;
	UAnimNextGraph_EdGraph* EdGraph = Cast<UAnimNextGraph_EdGraph>(ParentGraph);

	if (EditorData != nullptr && EdGraph != nullptr)
	{
		const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(Notation);
		if (Template == nullptr)
		{
			return nullptr;
		}

		const int32 NotationHash = (int32)GetTypeHash(Notation);
		const FString TemplateName = TEXT("RigVMTemplate_") + FString::FromInt(NotationHash);

		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(EditorData, Template->GetName().ToString());
		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		if (URigVMTemplateNode* ModelNode = Controller->AddTemplateNode(Notation, Location, Name.ToString(), true, false))
		{
			NewNode = Cast<UAnimNextGraph_EdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);
			}

			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->CancelUndoBracket();
		}
	}
	return NewNode;
}