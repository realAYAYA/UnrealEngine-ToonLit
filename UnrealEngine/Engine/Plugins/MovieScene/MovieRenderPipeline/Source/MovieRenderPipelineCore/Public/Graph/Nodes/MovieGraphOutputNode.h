﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphOutputNode.generated.h"

/** A graph node which displays all output members available in the graph. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphOutputNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphOutputNode();
	
	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

private:
	virtual void RegisterDelegates() const override;

	/** Register delegates for the provided output member. */
	void RegisterDelegates(UMovieGraphOutput* Output) const;

	/** Update data (name, etc) on all existing input pins on this node to reflect the output members on the graph. */
	void UpdateExistingPins(UMovieGraphMember* ChangedVariable) const;
};