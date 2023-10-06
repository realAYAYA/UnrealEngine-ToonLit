// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GraphElement.h"
#include "Graph/Graph.h"

UGraphElement::UGraphElement(EGraphElementType InElementType)
	: ElementType(InElementType)
{
}

UGraphElement::UGraphElement()
{
}

void UGraphElement::SetParentGraph(TObjectPtr<UGraph> InGraph)
{
	ParentGraph = InGraph;
}

TObjectPtr<UGraph> UGraphElement::GetGraph() const
{
	return ParentGraph.Get();
}