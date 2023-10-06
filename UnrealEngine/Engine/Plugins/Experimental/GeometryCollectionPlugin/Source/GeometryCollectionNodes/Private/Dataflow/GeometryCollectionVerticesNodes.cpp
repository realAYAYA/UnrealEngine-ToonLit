// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionVerticesNodes.h"
#include "GeometryCollection/GeometryCollection.h"

#include "Dataflow/DataflowCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionVerticesNodes)

namespace Dataflow
{
	void GeometryCollectionVerticesNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransformCollectionAttributeDataflowNode);
	}
}


void FTransformCollectionAttributeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		const FTransform& Transform = GetValue<FTransform>(Context, &TransformIn);
		FManagedArrayCollection CollectionValue = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (CollectionValue.FindAttributeTyped<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
		{
			auto UEVertd = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
			auto UEVertf = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };

			FTransform FinalTransform = LocalTransform * Transform;
			//FMatrix44d Matrix = (LocalTransform * Transform).ToMatrixWithScale();
			TManagedArray<FVector3f>& Vertices = CollectionValue.ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
			for (int32 i = 0; i < Vertices.Num(); i++)
			{
				//Vertices[i] = UEVertf(Matrix.TransformPosition(UEVertd(Vertices[i])));
				Vertices[i] = UEVertf(FinalTransform.TransformPosition(UEVertd(Vertices[i])));
			}
		}
		SetValue(Context, MoveTemp(CollectionValue), &Collection);

	}
}

