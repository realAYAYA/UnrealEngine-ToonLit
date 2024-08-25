// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieEdGraphNode.h"

#include "MoviePipelineEdGraphSubgraphNode.generated.h"


UCLASS()
class UMoviePipelineEdGraphSubgraphNode : public UMoviePipelineEdGraphNode
{
	GENERATED_BODY()

public:
	// ~Begin UEdGraphNode interface
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// ~End UEdGraphNode interface
};
