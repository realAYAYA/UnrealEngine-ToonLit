// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GraphHandle.h"
#include "Graph/GraphElement.h"
#include "Graph/GraphEdge.h"
#include "Graph/GraphIsland.h"
#include "Graph/GraphVertex.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraphHandle)

FGraphHandle::FGraphHandle()
{

}

FGraphHandle::FGraphHandle(int64 InUniqueIndex, TObjectPtr<UGraphElement> InElement)
	: UniqueIndex(InUniqueIndex)
	, Element(InElement)
{
}

void FGraphHandle::SetElement(TObjectPtr<UGraphElement> InElement)
{
	Element = InElement;
}

TObjectPtr<UGraphElement> FGraphHandle::GetElement() const
{
	return Element.Get();
}

bool FGraphHandle::IsValid() const
{
	return UniqueIndex != INDEX_NONE;
}

bool FGraphHandle::HasElement() const
{
	return Element.IsValid();
}

bool FGraphHandle::IsComplete() const
{
	return IsValid() && HasElement();
}

bool FGraphHandle::operator==(const FGraphHandle& Other) const
{
	return UniqueIndex == Other.UniqueIndex;
}

bool FGraphHandle::operator!=(const FGraphHandle& Other) const
{
	return UniqueIndex != Other.UniqueIndex;
}

bool FGraphHandle::operator<(const FGraphHandle& Other) const
{
	return UniqueIndex < Other.UniqueIndex;
}

void FGraphHandle::Clear()
{
	UniqueIndex = INDEX_NONE;
	Element = nullptr;
}

uint32 GetTypeHash(const FGraphHandle& Handle)
{
	return ::GetTypeHash(Handle.UniqueIndex);
}

FGraphVertexHandle::FGraphVertexHandle()
{
}

TObjectPtr<UGraphVertex> FGraphVertexHandle::GetVertex() const
{
	return Cast<UGraphVertex>(GetElement());
}

FGraphEdgeHandle::FGraphEdgeHandle()
{

}

TObjectPtr<UGraphEdge> FGraphEdgeHandle::GetEdge() const
{
	return Cast<UGraphEdge>(GetElement());
}

FGraphIslandHandle::FGraphIslandHandle()
{

}

TObjectPtr<UGraphIsland> FGraphIslandHandle::GetIsland() const
{
	return Cast<UGraphIsland>(GetElement());
}