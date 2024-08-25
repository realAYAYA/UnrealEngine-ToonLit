// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphBranchNode.generated.h"

/** 
* A node which creates a True/False branching condition. A user Graph Variable can be plugged
* into the conditional pin and this will be evaluated when flattening the graph, choosing which
* branch path to follow.
*/
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphBranchNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphBranchNode() = default;

	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetKeywords() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif
};