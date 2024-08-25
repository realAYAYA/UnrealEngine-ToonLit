// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "DataflowCollectionAddScalarVertexPropertyNode.generated.h"

/** Scalar vertex properties. */
USTRUCT(Meta = (DataflowCollection))
struct DATAFLOWNODES_API FDataflowCollectionAddScalarVertexPropertyNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowCollectionAddScalarVertexPropertyNode, "AddScalarVertexProperty", "Collection", "Add a saved scalar property to a collection")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Vertex Attribute", Meta = (DataflowOutput))
	FString Name;

	UPROPERTY()
	TArray<float> VertexWeights;

	FDataflowCollectionAddScalarVertexPropertyNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
