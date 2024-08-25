// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionVertexScalarToVertexIndicesNode.h"

#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionVertexScalarToVertexIndicesNode)
#define LOCTEXT_NAMESPACE "FGeometryCollectionVertexScalarToVertexIndicesNode"

FGeometryCollectionVertexScalarToVertexIndicesNode::FGeometryCollectionVertexScalarToVertexIndicesNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&VertexAttributeName);
	RegisterOutputConnection(&Indices);
}

void FGeometryCollectionVertexScalarToVertexIndicesNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA< TArray<int32> >(&Indices))
	{
		TArray<int32> IndicesOut;

		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FString VertixAttributeNameVal = GetValue<FString>(Context, &VertexAttributeName, VertexAttributeName);

		if( const TManagedArray<float>* FloatArray = InCollection.FindAttribute<float>(FName(VertixAttributeNameVal), FName("Vertices") ) )
		{
			for (int i = 0; i < FloatArray->Num(); i++)
			{
				if ((*FloatArray)[i] > SelectionThreshold)
				{
					IndicesOut.Add(i);
				}
			}
		}
		SetValue< TArray<int32> >(Context, MoveTemp(IndicesOut), &Indices);
	}
}


#undef LOCTEXT_NAMESPACE
