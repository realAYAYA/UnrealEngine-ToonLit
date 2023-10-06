﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphInputNode.generated.h"

/** A graph node which displays all input members available in the graph. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphInputNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphInputNode();

	//~ Begin UMovieGraphNode interface
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif
	//~ End UMovieGraphNode interface

private:
	virtual void RegisterDelegates() const override;

	/** Register delegates for the provided input member. */
	void RegisterDelegates(UMovieGraphInput* Input) const;

	/** Update data (name, etc) on all existing output pins on this node to reflect the input members on the graph. */
	void UpdateExistingPins(UMovieGraphMember* ChangedVariable) const;
};