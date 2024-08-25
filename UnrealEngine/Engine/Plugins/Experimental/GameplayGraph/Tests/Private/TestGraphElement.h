// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/GraphElement.h"

#include "TestGraphElement.generated.h"

UCLASS()
class UTestGraphElement : public UGraphElement
{
	GENERATED_BODY()

public:
	void PublicSetUniqueIndex(FGraphUniqueIndex InUniqueIndex)
	{
		SetUniqueIndex(InUniqueIndex);
	}

	FGraphUniqueIndex PublicGetUniqueIndex()
	{
		return GetUniqueIndex();
	}


	void PublicSetParentGraph(TObjectPtr<UGraph> InGraph)
	{
		SetParentGraph(InGraph);
	}

	TObjectPtr<UGraph> PublicGetGraph() const
	{
		return GetGraph();
	}
};
