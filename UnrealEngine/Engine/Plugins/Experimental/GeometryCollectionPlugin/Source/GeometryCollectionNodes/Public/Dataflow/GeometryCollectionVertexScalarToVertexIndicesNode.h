// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollectionVertexScalarToVertexIndicesNode.generated.h"

/** Convert an vertex float array to a list of indices */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGeometryCollectionVertexScalarToVertexIndicesNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGeometryCollectionVertexScalarToVertexIndicesNode, "VertexScalarToVertexIndices", "GeometryCollection", "Collection Vertex Weight Map to Indices")

public:

	UPROPERTY(Meta = (DataflowInput))
	FManagedArrayCollection Collection;

	/** The name of the vertex attribute to generate indices from. */
	UPROPERTY(EditAnywhere, Category = "Selection Filter", Meta = (DataflowInput))
	FString VertexAttributeName;

	/** The value threshold for what is included in the vertex list. */
	UPROPERTY(EditAnywhere, Category = "Selection Filter", Meta = (ClampMin = "0", ClampMax = "1"))
	float SelectionThreshold = 0.f;

	/** Output list of indices */
	UPROPERTY(Meta = (DataflowOutput))
	TArray<int32> Indices = {};

	FGeometryCollectionVertexScalarToVertexIndicesNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
