// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/AddWeightMapNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AddWeightMapNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetAddWeightMapNode"

FChaosClothAssetAddWeightMapNode::FChaosClothAssetAddWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FChaosClothAssetAddWeightMapNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Make the name a valid attribute name, and replace the value in the UI
		FWeightMapTools::MakeWeightMapName(Name);

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			const FName InName(Name);
			ClothFacade.AddWeightMap(InName);		// Does nothing if weight map already exists

			TArrayView<float> ClothWeights = ClothFacade.GetWeightMap(InName);
			const int32 MaxWeightIndex = FMath::Min(VertexWeights.Num(), ClothWeights.Num());
			if (VertexWeights.Num() > 0 && VertexWeights.Num() != ClothWeights.Num())
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("VertexCountMismatchHeadline", "Vertex count mismatch."),
					FText::Format(LOCTEXT("VertexCountMismatchDetails", "Vertex weights in the node: {0}\n3D vertices in the cloth: {1}"),
						VertexWeights.Num(),
						ClothWeights.Num()));
			}

			for (int32 VertexID = 0; VertexID < MaxWeightIndex; ++VertexID)
			{
				ClothWeights[VertexID] = VertexWeights[VertexID];
			}
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
		SetValue(Context, Name, &Name);
	}
}

#undef LOCTEXT_NAMESPACE
