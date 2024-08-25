// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeInput.h"

#include "PCGNode.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNodeInput"

FText UPCGEditorGraphNodeInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{	
	return LOCTEXT("NodeTitle", "Input");
}

void UPCGEditorGraphNodeInput::AllocateDefaultPins()
{
	if (PCGNode)
	{
		CreatePins(/*InInputPins=*/{}, PCGNode->GetOutputPins());
	}
}

void UPCGEditorGraphNodeInput::ReconstructNode()
{
	Super::ReconstructNode();
	//TODO: Implement special version for input to avoid the enum type
}

#undef LOCTEXT_NAMESPACE
