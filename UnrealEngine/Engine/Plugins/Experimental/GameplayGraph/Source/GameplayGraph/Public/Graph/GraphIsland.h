// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/GraphElement.h"
#include "GraphIsland.generated.h"

USTRUCT()
struct FSerializedIslandData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FGraphVertexHandle> Vertices;
};

/** Delegate to track when some sort of batch change has occurred on this island that probably changes its connectivity.
 *  This is different from FOnGraphIslandNodeRemoved since FOnGraphIslandDestructiveChangeFinish will only be called
 *  once for the graph for a given operation while FOnGraphIslandNodeRemoved may be called multiple times if we're removing
 *  more than one node from the island at a given time. Note that this will only be called as a result of a destructive change.
 *  So repeatedly adding a node to an island won't call this event. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphIslandConnectedComponentsChanged, const FGraphIslandHandle&);

/** Delegate to track when this island should no longer exist. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphIslandDestroyed, const FGraphIslandHandle&);

/** Delegate to track the event when the island has a node added to it. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGraphIslandVertexAdded, const FGraphIslandHandle&, const FGraphVertexHandle&);

/** Delegate to track the event when the island has a node removed from it. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGraphIslandVertexRemoved, const FGraphIslandHandle&, const FGraphVertexHandle&);

UCLASS()
class GAMEPLAYGRAPH_API UGraphIsland : public UGraphElement
{
	GENERATED_BODY()
public:
	UGraphIsland();

	FGraphIslandHandle Handle() const
	{
		return FGraphIslandHandle{ GetUniqueIndex(), const_cast<UGraphIsland*>(this) };
	}

	bool IsPendingDestroy() const { return bPendingDestroy; }
	bool IsEmpty() const { return Vertices.IsEmpty(); }
	const TSet<FGraphVertexHandle>& GetVertices() const { return Vertices; }
	FSerializedIslandData GetSerializedData() const;

	FOnGraphIslandVertexAdded OnVertexAdded;
	FOnGraphIslandVertexRemoved OnVertexRemoved;
	FOnGraphIslandDestroyed OnDestroyed;
	FOnGraphIslandConnectedComponentsChanged OnConnectivityChanged;

	friend class UGraph;
protected:

	virtual void HandleOnVertexAdded(const FGraphVertexHandle& Handle);
	virtual void HandleOnVertexRemoved(const FGraphVertexHandle& Handle);
	virtual void HandleOnDestroyed();
	virtual void HandleOnConnectivityChanged();

	/** Called when removing the island from the graph. */
	void Destroy();

	/** Takes the nodes from the other island and puts them into this island. */
	void MergeWith(TObjectPtr<UGraphIsland> OtherIsland);

	/** Adds a single node into this island. */
	void AddVertex(const FGraphVertexHandle& Node);

	/** Removes a node from the island. */
	void RemoveVertex(const FGraphVertexHandle& Node);

private:
	UPROPERTY(SaveGame)
	TSet<FGraphVertexHandle> Vertices;

	UPROPERTY(Transient)
	bool bPendingDestroy;
};