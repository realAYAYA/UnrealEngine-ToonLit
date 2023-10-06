// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieEdGraphVariableNode.h"

#include "EdGraph/EdGraphPin.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"

#define LOCTEXT_NAMESPACE "MoviePipelineGraph"

FText UMoviePipelineEdGraphVariableNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	static const FText VariableNodeTitle = LOCTEXT("GetVariableNodeTitle", "Get Variable");
	static const FText GlobalVariableNodeTitle = LOCTEXT("GetGlobalVariableNodeTitle", "Get Global Variable");

	const UMovieGraphVariableNode* VariableNode = Cast<UMovieGraphVariableNode>(RuntimeNode);

	return (VariableNode && VariableNode->IsGlobalVariable()) ? GlobalVariableNodeTitle : VariableNodeTitle;
}

void UMoviePipelineEdGraphVariableNode::AllocateDefaultPins()
{
	if (const UMovieGraphVariableNode* VariableNode = Cast<UMovieGraphVariableNode>(RuntimeNode))
	{
		const TArray<TObjectPtr<UMovieGraphPin>>& OutputPins = RuntimeNode->GetOutputPins();
		if (!OutputPins.IsEmpty())
		{
			CreatePin(EGPD_Output, GetPinType(OutputPins[0].Get()), FName(VariableNode->GetVariable()->GetMemberName()));
		}
	}
}

#undef LOCTEXT_NAMESPACE