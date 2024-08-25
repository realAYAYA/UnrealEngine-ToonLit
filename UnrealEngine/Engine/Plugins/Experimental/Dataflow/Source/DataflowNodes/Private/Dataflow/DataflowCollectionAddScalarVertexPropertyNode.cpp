// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowTools.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowCollectionAddScalarVertexPropertyNode)

#define LOCTEXT_NAMESPACE "DataflowCollectionAddScalarVertexProperty"

FDataflowCollectionAddScalarVertexPropertyNode::FDataflowCollectionAddScalarVertexPropertyNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FDataflowCollectionAddScalarVertexPropertyNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace Dataflow;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (!Name.IsEmpty())
		{
			FName nName(Name), nGroup("Vertices");
			TManagedArray<float>& Scalar = InCollection.AddAttribute<float>(nName,nGroup);

			const int32 MaxWeightIndex = FMath::Min(VertexWeights.Num(), Scalar.Num());
			if (VertexWeights.Num() > 0 && VertexWeights.Num() != Scalar.Num())
			{
				FDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("VertexCountMismatchHeadline", "Vertex count mismatch."),
					FText::Format(LOCTEXT("VertexCountMismatchDetails", "Vertex weights in the node: {0}\n3D vertices in the cloth: {1}"),
						VertexWeights.Num(),
						Scalar.Num()));
			}

			for (int32 VertexID = 0; VertexID < MaxWeightIndex; ++VertexID)
			{
				Scalar[VertexID] = VertexWeights[VertexID];
			}
		}

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
	else if (Out->IsA<FString>(&Name))
	{
		SetValue(Context, Name, &Name);
	}
}

#undef LOCTEXT_NAMESPACE
