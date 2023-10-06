// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "Misc/Optional.h"
#include "VisualGraphElement.h"

class FVisualGraph;

class VISUALGRAPHUTILS_API FVisualGraphNode : public FVisualGraphElement
{
public:

	FVisualGraphNode()
	: FVisualGraphElement()
	, Shape()
	, SubGraphIndex(INDEX_NONE)
	{}

	virtual ~FVisualGraphNode() override {}

	TOptional<EVisualGraphShape> GetShape() const { return Shape; }
	void SetShape(EVisualGraphShape InValue) { Shape = InValue; }

protected:

	virtual FString DumpDot(const FVisualGraph* InGraph, int32 InIndendation) const override;

	TOptional<EVisualGraphShape> Shape;
	int32 SubGraphIndex;

	friend class FVisualGraph;
	friend class FVisualGraphSubGraph;
};
