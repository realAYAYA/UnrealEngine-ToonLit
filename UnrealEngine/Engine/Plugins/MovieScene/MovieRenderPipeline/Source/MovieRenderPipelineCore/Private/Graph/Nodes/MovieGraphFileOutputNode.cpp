// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "Graph/MovieGraphConfig.h"

int32 UMovieGraphFileOutputNode::GetNumFileOutputNodes(const UMovieGraphEvaluatedConfig& InEvaluatedConfig, const FName& InBranchName)
{
	return InEvaluatedConfig.GetSettingsForBranch(UMovieGraphFileOutputNode::StaticClass(), InBranchName, false /*bIncludeCDOs*/, false /*bExactMatch*/).Num();
}
