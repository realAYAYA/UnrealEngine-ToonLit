// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineEdGraphSubgraphNode.h"

#include "Graph/Nodes/MovieGraphSubgraphNode.h"

UObject* UMoviePipelineEdGraphSubgraphNode::GetJumpTargetForDoubleClick() const
{
	if (const UMovieGraphSubgraphNode* SubgraphNode = Cast<UMovieGraphSubgraphNode>(RuntimeNode))
	{
		if (UMovieGraphConfig* SubgraphAsset =  SubgraphNode->GetSubgraphAsset())
		{
			return SubgraphAsset;
		}
	}
	
	return Super::GetJumpTargetForDoubleClick();
}
