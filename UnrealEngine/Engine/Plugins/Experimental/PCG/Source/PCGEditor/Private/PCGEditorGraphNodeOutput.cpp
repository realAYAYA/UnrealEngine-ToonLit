// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeOutput.h"

#include "PCGNode.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNodeOutput"

FText UPCGEditorGraphNodeOutput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName(TEXT("Output"));
}

void UPCGEditorGraphNodeOutput::AllocateDefaultPins()
{
	if (PCGNode)
	{
		for (const UPCGPin* InputPin : PCGNode->GetInputPins())
		{
			CreatePin(EEdGraphPinDirection::EGPD_Input, GetPinType(InputPin), InputPin->Properties.Label);
		}
	}
}

void UPCGEditorGraphNodeOutput::ReconstructNode()
{
	Super::ReconstructNode();
	//TODO: Implement special version for output to avoid the enum type
}

#undef LOCTEXT_NAMESPACE
