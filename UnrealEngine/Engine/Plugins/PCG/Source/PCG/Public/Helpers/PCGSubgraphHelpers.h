// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class UPCGGraph;
class UPCGNode;
class UObject;

class FPCGSubgraphHelpers
{
public:
	static PCG_API UPCGGraph* CollapseIntoSubgraphWithReason(UPCGGraph* InOriginalGraph, const TArray<UPCGNode*>& InNodesToCollapse, const TArray<UObject*>& InExtraNodesToCollapse, FText& OutFailReason, UPCGGraph* OptionalPreAllocatedGraph = nullptr);
	static PCG_API UPCGGraph* CollapseIntoSubgraph(UPCGGraph* InOriginalGraph, const TArray<UPCGNode*>& InNodesToCollapse, const TArray<UObject*>& InExtraNodesToCollapse, UPCGGraph* OptionalPreAllocatedGraph = nullptr);
};