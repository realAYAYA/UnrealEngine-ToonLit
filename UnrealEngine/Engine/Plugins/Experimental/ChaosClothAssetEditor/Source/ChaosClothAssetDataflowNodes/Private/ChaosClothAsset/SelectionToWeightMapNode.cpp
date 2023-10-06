// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SelectionToWeightMapNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectionToWeightMapNode)
#define LOCTEXT_NAMESPACE "FChaosClothAssetSelectionToWeightMapNode"

FChaosClothAssetSelectionToWeightMapNode::FChaosClothAssetSelectionToWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterInputConnection(&SelectionName);
	RegisterOutputConnection(&WeightMapName);
}


void FChaosClothAssetSelectionToWeightMapNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);

		const FCollectionClothSelectionConstFacade SelectionFacade(ClothCollection);
		const FString InSelectionName = GetValue<FString>(Context, &SelectionName);

		if (SelectionFacade.IsValid() && ClothFacade.IsValid())
		{
			const int32 FoundSelectionIndex = SelectionFacade.FindSelection(InSelectionName);

			if (FoundSelectionIndex != INDEX_NONE)
			{
				const FString& SelectionType = SelectionFacade.GetType()[FoundSelectionIndex];

				if (SelectionType == "SimVertex3D" || SelectionType == "SimVertex2D" || SelectionType == "SimFace")
				{
					const FName InMapName(WeightMapName);
					ClothFacade.AddWeightMap(InMapName);
					TArrayView<float> OutClothWeights = ClothFacade.GetWeightMap(InMapName);

					const TSet<int32>& Selection = SelectionFacade.GetIndices()[FoundSelectionIndex];

					if (SelectionType == "SimVertex3D")
					{
						for (int32 VertexIndex = 0; VertexIndex < OutClothWeights.Num(); ++VertexIndex)
						{
							OutClothWeights[VertexIndex] = Selection.Contains(VertexIndex) ? SelectedValue : UnselectedValue;
						}
					}
					else if (SelectionType == "SimVertex2D")
					{
						// We are given a selection over the set of 2D vertices, but weight maps only exist for 3D vertices, so
						// we need a bit of translation

						const TConstArrayView<TArray<int32>> Vertex3DTo2D = ClothFacade.GetSimVertex2DLookup();

						for (int32 Vertex3DIndex = 0; Vertex3DIndex < OutClothWeights.Num(); ++Vertex3DIndex)
						{
							// If any corresponding 2D vertex is selected, set the 3D weight map value to one, otherwise it gets zero
							OutClothWeights[Vertex3DIndex] = UnselectedValue;
							for (const int32 Vertex2DIndex : Vertex3DTo2D[Vertex3DIndex])
							{
								if (Selection.Contains(Vertex2DIndex))
								{
									OutClothWeights[Vertex3DIndex] = SelectedValue;
									break;
								}
							}
						}
					}
					else
					{
						check(SelectionType == "SimFace");
						// Fill with unselected value and then set all vertices in the selected faces to the selected value.
						for (int32 Vertex3DIndex = 0; Vertex3DIndex < OutClothWeights.Num(); ++Vertex3DIndex)
						{
							OutClothWeights[Vertex3DIndex] = UnselectedValue;
						}
						const TConstArrayView<FIntVector3> SimIndices3D = ClothFacade.GetSimIndices3D();
						for (const int32 FaceIndex : Selection)
						{
							OutClothWeights[SimIndices3D[FaceIndex][0]] = SelectedValue;
							OutClothWeights[SimIndices3D[FaceIndex][1]] = SelectedValue;
							OutClothWeights[SimIndices3D[FaceIndex][2]] = SelectedValue;
						}
					}
				}
				else
				{
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("SelectionTypeNotCorrectHeadline", "Selection type is incompatible."),
						FText::Format(LOCTEXT("SelectionTypeNotCorrectDetails", "Selection with Name \"{0}\" does not have Type \"SimVertex3D\", \"SimVertex2D\", or \"SimFace\"."), FText::FromString(InSelectionName)));
				}
			}
			else
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("SelectionNameNotFoundHeadline", "Selection Name was not found."),
					FText::Format(LOCTEXT("SelectionNameNotFoundDetails", "A Selection with Name \"{0}\" was not found in the Collection."),
						FText::FromString(InSelectionName)));
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
		SetValue(Context, WeightMapName, &WeightMapName);
	}
}


#undef LOCTEXT_NAMESPACE
