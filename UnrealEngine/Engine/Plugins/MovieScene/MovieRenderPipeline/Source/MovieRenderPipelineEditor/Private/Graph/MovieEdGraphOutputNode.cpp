// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieEdGraphOutputNode.h"

#define LOCTEXT_NAMESPACE "MoviePipelineGraph"

FText UMoviePipelineEdGraphNodeOutput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	static const FText NodeTitle = LOCTEXT("OutputsNodeTitle", "Outputs");
	return NodeTitle;
}

void UMoviePipelineEdGraphNodeOutput::AllocateDefaultPins()
{
	if (RuntimeNode)
	{
		CreatePins(RuntimeNode->GetInputPins(), /*InOutputPins=*/{});
	}
}
#undef LOCTEXT_NAMESPACE