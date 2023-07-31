// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeInput.h"

#include "PCGNode.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNodeInput"

FText UPCGEditorGraphNodeInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{	
	return FText::FromName(TEXT("Input"));
}

void UPCGEditorGraphNodeInput::AllocateDefaultPins()
{
	if (PCGNode)
	{
		for (const UPCGPin* OutPin : PCGNode->GetOutputPins())
		{
			CreatePin(EEdGraphPinDirection::EGPD_Output, GetPinType(OutPin), OutPin->Properties.Label);
		}
	}
}

void UPCGEditorGraphNodeInput::ReconstructNode()
{
	Super::ReconstructNode();
	//TODO: Implement special version for input to avoid the enum type
}

#undef LOCTEXT_NAMESPACE
