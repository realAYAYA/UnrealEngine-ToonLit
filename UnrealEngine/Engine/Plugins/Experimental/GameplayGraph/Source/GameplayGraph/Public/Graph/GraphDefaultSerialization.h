// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/Graph.h"
#include "Graph/GraphSerialization.h"

#include <type_traits>

#include "GraphDefaultSerialization.generated.h"

USTRUCT()
struct FSerializedEdgeData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGraphVertexHandle Node1;

	UPROPERTY(SaveGame)
	FGraphVertexHandle Node2;

	// Comparison operators
	friend bool operator==(const FSerializedEdgeData& Lhs, const FSerializedEdgeData& Rhs) = default;
	friend bool operator!=(const FSerializedEdgeData& Lhs, const FSerializedEdgeData& Rhs) = default;
};

USTRUCT()
struct FSerializedIslandData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FGraphVertexHandle> Vertices;

	// Comparison operators
	friend bool operator==(const FSerializedIslandData& Lhs, const FSerializedIslandData& Rhs) = default;
	friend bool operator!=(const FSerializedIslandData& Lhs, const FSerializedIslandData& Rhs) = default;
};

/**
 * The minimum amount of data we need to serialize to be able to reconstruct the graph as it was.
 * Note that classes that inherit from UGraph and its elements will no doubt want to extend the graph
 * with actual information on each node/edge/island. In that case, they should extend FSerializableGraph
 * to contain the extra information per graph handle. Furthermore, they'll need to extend UGraph to have
 * its own typed serialization save/load functions that call the base functions in UGraph first.
 */
USTRUCT()
struct FSerializableGraph
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGraphProperties Properties;

	UPROPERTY(SaveGame)
	TArray<FGraphVertexHandle> Vertices;

	UPROPERTY(SaveGame)
	TArray<FSerializedEdgeData> Edges;

	UPROPERTY(SaveGame)
	TMap<FGraphIslandHandle, FSerializedIslandData> Islands;
};

template<typename TSerializableGraph>
class TDefaultGraphSerialization : public IGraphSerialization
{
public:
	virtual ~TDefaultGraphSerialization() = default;

	const TSerializableGraph& GetData() const { return Data; }

	virtual void Initialize(int32 NumVertices, int32 NumEdges, int32 NumIslands) override
	{
		Data.Vertices.Reserve(NumVertices);
		Data.Edges.Reserve(NumEdges);
		Data.Islands.Reserve(NumIslands);
	}

	virtual void WriteGraphProperties(const FGraphProperties& Properties) override
	{
		Data.Properties = Properties;
	}

	virtual void WriteGraphVertex(const FGraphVertexHandle& VertexHandle, const UGraphVertex* Vertex) override
	{
		Data.Vertices.Add(VertexHandle);
	}

	virtual void WriteGraphEdge(const FGraphVertexHandle& VertexHandleA, const FGraphVertexHandle& VertexHandleB) override
	{
		FSerializedEdgeData Serialized;
		Serialized.Node1 = VertexHandleA;
		Serialized.Node2 = VertexHandleB;
		Data.Edges.Emplace(MoveTemp(Serialized));
	}

	virtual void WriteGraphIsland(const FGraphIslandHandle& IslandHandle, const UGraphIsland* Island) override
	{
		if (!ensure(Island))
		{
			return;
		}

		FSerializedIslandData Serialized;
		for (const FGraphVertexHandle& Handle : Island->GetVertices())
		{
			Serialized.Vertices.Add(Handle);
		}

		Data.Islands.Emplace(IslandHandle, MoveTemp(Serialized));
	}

protected:
	TSerializableGraph Data;
};

template<typename TSerializableGraph>
class TDefaultGraphDeserialization : public IGraphDeserialization
{
public:
	explicit TDefaultGraphDeserialization(const TSerializableGraph& InData)
	: Data(InData)
	{
	}
	virtual ~TDefaultGraphDeserialization() = default;

	virtual const FGraphProperties& GetProperties() const override
	{
		return Data.Properties;
	}

	virtual int32 NumVertices() const override
	{
		return Data.Vertices.Num();
	}

	virtual void ForEveryVertex(const TFunction<FGraphVertexHandle(const FGraphVertexHandle&)>& Lambda) const override
	{
		for (const FGraphVertexHandle& SerializedHandle : Data.Vertices)
		{
			const FGraphVertexHandle FinalHandle = Lambda(SerializedHandle);
			if (FinalHandle.IsValid())
			{
				OnDeserializedVertex(FinalHandle);
			}
		}
	}

	virtual int32 NumEdges() const override
	{
		return Data.Edges.Num();
	}

	virtual void ForEveryEdge(const TFunction<bool(const FEdgeSpecifier&)>& Lambda) const override
	{
		for (const FSerializedEdgeData& Serialized : Data.Edges)
		{
			const FEdgeSpecifier Construction{ Serialized.Node1, Serialized.Node2 };
			if (Lambda(Construction))
			{
				OnDeserializedEdge(Construction);
			}
		}
	}

	virtual int32 NumIslands() const override
	{
		return Data.Islands.Num();
	}

	virtual void ForEveryIsland(const TFunction<FGraphIslandHandle(const FGraphIslandHandle&, const FIslandConstructionData&)>& Lambda) const override
	{
		for (const TPair<FGraphIslandHandle, FSerializedIslandData>& Serialized : Data.Islands)
		{
			if (Serialized.Value.Vertices.IsEmpty())
			{
				continue;
			}

			FIslandConstructionData Construction;
			Construction.Vertices = Serialized.Value.Vertices;

			const FGraphIslandHandle FinalHandle = Lambda(Serialized.Key, Construction);
			if (FinalHandle.IsValid())
			{
				OnDeserializedIsland(FinalHandle);
			}
		}
	}

protected:
	virtual void OnDeserializedVertex(const FGraphVertexHandle& VertexHandle) const {}
	virtual void OnDeserializedEdge(const FEdgeSpecifier& Edge) const {}
	virtual void OnDeserializedIsland(const FGraphIslandHandle& IslandHandle) const {}

	const TSerializableGraph& Data;
};

using FDefaultGraphSerialization = TDefaultGraphSerialization<FSerializableGraph>;
using FDefaultGraphDeserialization = TDefaultGraphDeserialization<FSerializableGraph>;