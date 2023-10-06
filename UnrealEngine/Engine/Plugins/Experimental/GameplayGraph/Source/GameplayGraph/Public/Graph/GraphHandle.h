// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "GraphHandle.generated.h"

class UGraphElement;
class UGraphVertex;
class UGraphEdge;
class UGraphIsland;

/**
 * For persistence, every node in a graph is given a unique index.
 * A FGraphHandle encapsulates that index to make it easy to go from
 * the index to the node and vice versa.
 */
USTRUCT()
struct GAMEPLAYGRAPH_API FGraphHandle
{
	GENERATED_BODY()
public:
	FGraphHandle();
	FGraphHandle(int64 InUniqueIndex, TObjectPtr<UGraphElement> InElement);

	void Clear();

	/** Whether or not this handle has been initialized. */
	bool IsValid() const;
	bool HasElement() const;
	bool IsComplete() const;

	int64 GetUniqueIndex() const { return UniqueIndex; }

	void SetElement(TObjectPtr<UGraphElement> InElement);
	TObjectPtr<UGraphElement> GetElement() const;

	bool operator==(const FGraphHandle& Other) const;
	bool operator!=(const FGraphHandle& Other) const;
	bool operator<(const FGraphHandle& Other) const;

	friend uint32 GAMEPLAYGRAPH_API GetTypeHash(const FGraphHandle& Handle);
private:
	/** Unique identifier within a graph. */
	UPROPERTY(SaveGame)
	int64 UniqueIndex = INDEX_NONE;

	/** Pointer to the graph */
	UPROPERTY(Transient)
	TWeakObjectPtr<UGraphElement> Element;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphVertexHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphVertexHandle();
	FGraphVertexHandle(int64 InUniqueIndex, TObjectPtr<UGraphElement> InElement)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphVertex> GetVertex() const;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphEdgeHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphEdgeHandle();
	FGraphEdgeHandle(int64 InUniqueIndex, TObjectPtr<UGraphElement> InElement)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphEdge> GetEdge() const;
};

USTRUCT()
struct GAMEPLAYGRAPH_API FGraphIslandHandle : public FGraphHandle
{
	GENERATED_BODY()

	FGraphIslandHandle();
	FGraphIslandHandle(int64 InUniqueIndex, TObjectPtr<UGraphElement> InElement)
		: FGraphHandle(InUniqueIndex, InElement)
	{}

	TObjectPtr<UGraphIsland> GetIsland() const;
};