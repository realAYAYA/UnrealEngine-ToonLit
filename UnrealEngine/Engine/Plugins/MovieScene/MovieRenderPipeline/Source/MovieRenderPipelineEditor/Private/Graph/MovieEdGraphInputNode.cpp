// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieEdGraphInputNode.h"

#define LOCTEXT_NAMESPACE "MoviePipelineGraph"

FText UMoviePipelineEdGraphNodeInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	static const FText NodeTitle = LOCTEXT("InputsNodeTitle", "Inputs");
	return NodeTitle;
}

void UMoviePipelineEdGraphNodeInput::AllocateDefaultPins()
{
	if (RuntimeNode)
	{
		CreatePins(/*InInputPins=*/{}, RuntimeNode->GetOutputPins());
	}
}

#undef LOCTEXT_NAMESPACE