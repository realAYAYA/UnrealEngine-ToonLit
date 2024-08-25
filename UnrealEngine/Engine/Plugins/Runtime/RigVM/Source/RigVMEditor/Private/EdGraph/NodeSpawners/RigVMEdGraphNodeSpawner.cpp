// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphNodeSpawner.h"
#include "RigVMStringUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphNodeSpawner)

#define LOCTEXT_NAMESPACE "RigVMEdGraphNodeSpawner"

bool URigVMEdGraphNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	check(RelatedBlueprintClass);

	for(const UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if(Blueprint->GetClass() != RelatedBlueprintClass)
		{
			return true;
		}
	}

	return false;
}

void URigVMEdGraphNodeSpawner::SetRelatedBlueprintClass(TSubclassOf<URigVMBlueprint> InClass)
{
	RelatedBlueprintClass = InClass;
}

URigVMEdGraphNode* URigVMEdGraphNodeSpawner::SpawnTemplateNode(UEdGraph* InParentGraph, const TArray<FPinInfo>& InPins, const FName& InNodeName)
{
	URigVMEdGraphNode* NewNode = NewObject<URigVMEdGraphNode>(InParentGraph, InNodeName);
	InParentGraph->AddNode(NewNode, false);

	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();

	for(const FPinInfo& PinInfo : InPins)
	{
		const FName PinName = *RigVMStringUtils::JoinPinPath(NewNode->GetName(), PinInfo.Name.ToString());
		
		if(PinInfo.Direction ==  ERigVMPinDirection::Input ||
			PinInfo.Direction ==  ERigVMPinDirection::IO)
		{
			UEdGraphPin* InputPin = UEdGraphPin::CreatePin(NewNode);
			InputPin->PinName = PinName;
			NewNode->Pins.Add(InputPin);

			InputPin->Direction = EGPD_Input;
			InputPin->PinType = RigVMTypeUtils::PinTypeFromCPPType(PinInfo.CPPType, PinInfo.CPPTypeObject);
		}

		if(PinInfo.Direction ==  ERigVMPinDirection::Output ||
			PinInfo.Direction ==  ERigVMPinDirection::IO)
		{
			UEdGraphPin* OutputPin = UEdGraphPin::CreatePin(NewNode);
			OutputPin->PinName = PinName;
			NewNode->Pins.Add(OutputPin);

			OutputPin->Direction = EGPD_Output;
			OutputPin->PinType = RigVMTypeUtils::PinTypeFromCPPType(PinInfo.CPPType, PinInfo.CPPTypeObject);
		}
	}

	NewNode->SetFlags(RF_Transactional);

	return NewNode;
}

#undef LOCTEXT_NAMESPACE

