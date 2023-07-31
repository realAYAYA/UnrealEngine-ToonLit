// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "VisualGraphElement.h"

#include "VisualGraphEdge.generated.h"

class FVisualGraph;

UENUM()
enum class EVisualGraphEdgeDirection : uint8
{
	SourceToTarget,
	TargetToSource,
	BothWays
};

class VISUALGRAPHUTILS_API FVisualGraphEdge : public FVisualGraphElement
{
public:

	FVisualGraphEdge()
	: FVisualGraphElement()
	, Direction(EVisualGraphEdgeDirection::SourceToTarget)
	{}

	virtual ~FVisualGraphEdge() override {}

	int32 GetSourceNode() const { return SourceNode; }
	int32 GetTargetNode() const { return TargetNode; }
	EVisualGraphEdgeDirection GetDirection() const { return Direction; }

protected:

	virtual FString DumpDot(const FVisualGraph* InGraph, int32 InIndendation) const override;

	int32 SourceNode;
	int32 TargetNode;
	EVisualGraphEdgeDirection Direction;

	friend class FVisualGraph;
	friend class FVisualGraphSubGraph;
};

