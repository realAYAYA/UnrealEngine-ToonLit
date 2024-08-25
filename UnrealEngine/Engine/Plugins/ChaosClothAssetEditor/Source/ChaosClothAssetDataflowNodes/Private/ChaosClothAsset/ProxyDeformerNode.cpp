// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ProxyDeformerNode.h"
#include "ChaosClothAsset/ClothCollectionAttribute.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Utils/ClothingMeshUtils.h"
#include "PointWeightMap.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyDeformerNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetProxyDeformerNode"

namespace UE::Chaos::ClothAsset::Private
{
	struct FDeformerMappingDataGenerator
	{
		TConstArrayView<FVector3f> SimPositions;
		TConstArrayView<FIntVector3> SimIndices;
		TConstArrayView<FVector3f> RenderPositions;
		TConstArrayView<FVector3f> RenderNormals;
		TConstArrayView<FIntVector3> RenderIndices;
		FPointWeightMap PointWeightMap;
		TArray<ClothingMeshUtils::FMeshToMeshFilterSet> MeshToMeshFilterSet;

		TArrayView<TArray<FVector4f>> RenderDeformerPositionBaryCoordsAndDist;
		TArrayView<TArray<FVector4f>> RenderDeformerNormalBaryCoordsAndDist;
		TArrayView<TArray<FVector4f>> RenderDeformerTangentBaryCoordsAndDist;
		TArrayView<TArray<FIntVector3>> RenderDeformerSimIndices3D;
		TArrayView<TArray<float>> RenderDeformerWeight;
		TArrayView<float> RenderDeformerSkinningBlend;

		int32 Generate(bool bUseSmoothTransition, bool bUseMultipleInfluences, float InfluenceRadius)
		{
			check(RenderPositions.Num() == RenderNormals.Num());
			check(RenderPositions.Num() == RenderDeformerPositionBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerNormalBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerTangentBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerSimIndices3D.Num());
			check(RenderPositions.Num() == RenderDeformerWeight.Num());
			check(RenderPositions.Num() == RenderDeformerSkinningBlend.Num());

			if (!ensureMsgf(SimPositions.Num() <= (int32)TNumericLimits<uint16>::Max() + 1, TEXT("FMeshToMeshVertData data is limited to 16bit unsigned int indexes (65536 indices max).")))
			{
				return 0;
			}

			TArray<uint32> ScalarSimIndices;
			ScalarSimIndices.Reserve(SimIndices.Num() * 3);
			for (const FIntVector3& SimIndex : SimIndices)
			{
				ScalarSimIndices.Add(SimIndex[0]);
				ScalarSimIndices.Add(SimIndex[1]);
				ScalarSimIndices.Add(SimIndex[2]);
			}
			TArray<uint32> ScalarRenderIndices;
			ScalarRenderIndices.Reserve(RenderIndices.Num() * 3);
			for (const FIntVector3& RenderIndex : RenderIndices)
			{
				ScalarRenderIndices.Add(RenderIndex[0]);
				ScalarRenderIndices.Add(RenderIndex[1]);
				ScalarRenderIndices.Add(RenderIndex[2]);
			}

			const ClothingMeshUtils::ClothMeshDesc SimMeshDesc(SimPositions, ScalarSimIndices);
			const ClothingMeshUtils::ClothMeshDesc RenderMeshDesc(RenderPositions, RenderNormals, ScalarRenderIndices);

			TArray<FMeshToMeshVertData> MeshToMeshVertData;

			ClothingMeshUtils::GenerateMeshToMeshVertData(
				MeshToMeshVertData,
				RenderMeshDesc,
				SimMeshDesc,
				&PointWeightMap,
				bUseSmoothTransition,
				bUseMultipleInfluences,
				InfluenceRadius,
				MeshToMeshFilterSet);

			const int32 NumInfluences = MeshToMeshVertData.Num() / RenderPositions.Num();
			check(MeshToMeshVertData.Num() == RenderPositions.Num() * NumInfluences);  // Check modulo
			check((!bUseMultipleInfluences && NumInfluences == 1) || (bUseMultipleInfluences && NumInfluences > 1));

			for (int32 Index = 0; Index < RenderPositions.Num(); ++Index)
			{
				RenderDeformerPositionBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerNormalBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerTangentBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerSimIndices3D[Index].SetNum(NumInfluences);
				RenderDeformerWeight[Index].SetNum(NumInfluences);

				RenderDeformerSkinningBlend[Index] = 0.f;

				for (int32 Influence = 0; Influence < NumInfluences; ++Influence)
				{
					const FMeshToMeshVertData& MeshToMeshVertDatum = MeshToMeshVertData[Index * NumInfluences + Influence];

					RenderDeformerPositionBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.PositionBaryCoordsAndDist;
					RenderDeformerNormalBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.NormalBaryCoordsAndDist;
					RenderDeformerTangentBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.TangentBaryCoordsAndDist;
					RenderDeformerSimIndices3D[Index][Influence] = FIntVector3(
						MeshToMeshVertDatum.SourceMeshVertIndices[0],
						MeshToMeshVertDatum.SourceMeshVertIndices[1],
						MeshToMeshVertDatum.SourceMeshVertIndices[2]);
					RenderDeformerWeight[Index][Influence] = MeshToMeshVertDatum.Weight;

					RenderDeformerSkinningBlend[Index] += MeshToMeshVertDatum.Weight * (float)MeshToMeshVertDatum.SourceMeshVertIndices[3] / (float)TNumericLimits<uint16>::Max();
				}
			}
			return NumInfluences;
		}
	};

	static FPointWeightMap SelectionToPointWeightMap(const FCollectionClothConstFacade& ClothFacade, const FCollectionClothSelectionConstFacade& SelectionFacade, const FName& SelectionName)
	{
		constexpr float SelectedValue = 1.f;
		if (const TSet<int32>* SelectionSet = SelectionFacade.IsValid() ? SelectionFacade.FindSelectionSet(SelectionName) : nullptr)
		{
			FPointWeightMap PointWeightMap;

			const FName SelectionGroup = SelectionFacade.GetSelectionGroup(SelectionName);

			if (SelectionGroup == ClothCollectionGroup::SimVertices3D)
			{
				PointWeightMap.Initialize(ClothFacade.GetNumSimVertices3D());  // Init to zero (unselected)
				for (const int32 VertexIndex : *SelectionSet)
				{
					PointWeightMap[VertexIndex] = SelectedValue;
				}
				return PointWeightMap;
			}
			else if (SelectionGroup == ClothCollectionGroup::SimVertices2D)
			{
				PointWeightMap.Initialize(ClothFacade.GetNumSimVertices3D());  // Init to zero (unselected)
				const TConstArrayView<int32> Vertex2DTo3D = ClothFacade.GetSimVertex3DLookup();
				for (const int32 VertexIndex : *SelectionSet)
				{
					PointWeightMap[Vertex2DTo3D[VertexIndex]] = SelectedValue;
				}
				return PointWeightMap;
			}
			else if (SelectionGroup == ClothCollectionGroup::SimFaces)
			{
				PointWeightMap.Initialize(ClothFacade.GetNumSimVertices3D());  // Init to zero (unselected)
				const TConstArrayView<FIntVector3> SimIndices3D = ClothFacade.GetSimIndices3D();
				for (const int32 FaceIndex : *SelectionSet)
				{
					PointWeightMap[SimIndices3D[FaceIndex][0]] = SelectedValue;
					PointWeightMap[SimIndices3D[FaceIndex][1]] = SelectedValue;
					PointWeightMap[SimIndices3D[FaceIndex][2]] = SelectedValue;
				}
				return PointWeightMap;
			}
		}
		// Invalid or no selection found, all points are dynamic 
		return FPointWeightMap(ClothFacade.GetNumSimVertices3D(), SelectedValue);
	}

	static TArray<ClothingMeshUtils::FMeshToMeshFilterSet> SelectionsToMeshToMeshFilterSets(const FCollectionClothConstFacade& ClothFacade, const FCollectionClothSelectionConstFacade& SelectionFacade, const TArray<FName>& SelectionNames)
	{
		auto GetSimFaceSelection = [&ClothFacade](const FName& SelectionGroup, const TSet<int32>& SelectionSet) -> TSet<int32>
			{
				TSet<int32> SimFaceSelection;
				if (SelectionGroup == ClothCollectionGroup::SimVertices2D)
				{
					const TConstArrayView<FIntVector3> SimIndices2D = ClothFacade.GetSimIndices2D();
					SimFaceSelection.Reserve(SelectionSet.Num());

					for (int32 FaceIndex = 0; FaceIndex < SimIndices2D.Num(); ++FaceIndex)
					{
						const FIntVector3& Indices = SimIndices2D[FaceIndex];

						if (SelectionSet.Contains(Indices[0]) &&
							SelectionSet.Contains(Indices[1]) &&
							SelectionSet.Contains(Indices[2]))
						{
							SimFaceSelection.Add(FaceIndex);
						}
					}
				}
				else if (SelectionGroup == ClothCollectionGroup::SimVertices3D)
				{
					const TConstArrayView<FIntVector3> SimIndices3D = ClothFacade.GetSimIndices3D();
					SimFaceSelection.Reserve(SelectionSet.Num());

					for (int32 FaceIndex = 0; FaceIndex < SimIndices3D.Num(); ++FaceIndex)
					{
						const FIntVector3& Indices = SimIndices3D[FaceIndex];

						if (SelectionSet.Contains(Indices[0]) &&
							SelectionSet.Contains(Indices[1]) &&
							SelectionSet.Contains(Indices[2]))
						{
							SimFaceSelection.Add(FaceIndex);
						}
					}
				}
				else if (SelectionGroup == ClothCollectionGroup::SimFaces)
				{
					SimFaceSelection = SelectionSet;
				}
				return SimFaceSelection;
			};

		auto GetRenderVertexSelection = [&ClothFacade](const FName& SelectionGroup, const TSet<int32>& SelectionSet) -> TSet<int32>
			{
				TSet<int32> RenderVertexSelection;
				if (SelectionGroup == ClothCollectionGroup::RenderVertices)
				{
					RenderVertexSelection = SelectionSet;
				}
				else if (SelectionGroup == ClothCollectionGroup::RenderFaces)
				{
					RenderVertexSelection.Reserve(SelectionSet.Num() * 3);
					const TConstArrayView<FIntVector3> RenderIndices = ClothFacade.GetRenderIndices();
					for (const int32 FaceIndex : SelectionSet)
					{
						RenderVertexSelection.Add(RenderIndices[FaceIndex][0]);
						RenderVertexSelection.Add(RenderIndices[FaceIndex][1]);
						RenderVertexSelection.Add(RenderIndices[FaceIndex][2]);
					}
				}
				return RenderVertexSelection;
			};

		// Fill up the MeshToMeshFilterSets
		TArray<ClothingMeshUtils::FMeshToMeshFilterSet> MeshToMeshFilterSets;

		if (SelectionFacade.IsValid())
		{
			MeshToMeshFilterSets.Reserve(SelectionNames.Num());

			for (const FName& SelectionName : SelectionNames)
			{
				if (const TSet<int32>* SelectionSet = SelectionFacade.FindSelectionSet(SelectionName))
				{
					if (const TSet<int32>* SecondarySelectionSet = SelectionFacade.FindSelectionSecondarySet(SelectionName))
					{
						if (SelectionSet->Num() && SecondarySelectionSet->Num())
						{
							FName SelectionGroup = SelectionFacade.GetSelectionGroup(SelectionName);
							FName SelectionSecondaryGroup = SelectionFacade.GetSelectionSecondaryGroup(SelectionName);

							// Retrieve the sim face selection
							TSet<int32> SimFaceSelection = GetSimFaceSelection(SelectionGroup, *SelectionSet);
							if (!SimFaceSelection.Num())
							{
								// Try swapping the selections
								Swap(SelectionSet, SecondarySelectionSet);
								Swap(SelectionGroup, SelectionSecondaryGroup);

								SimFaceSelection = GetSimFaceSelection(SelectionGroup, *SelectionSet);
							}
							if (!SimFaceSelection.Num())
							{
								continue;  // Nothing selected on the simulation side
							}

							// Retrieve the render vertex selection
							TSet<int32> RenderVertexSelection = GetRenderVertexSelection(SelectionSecondaryGroup, *SecondarySelectionSet);
							if (!RenderVertexSelection.Num())
							{
								continue;  // Nothing selected on the render side
							}

							ClothingMeshUtils::FMeshToMeshFilterSet& MeshToMeshFilterSet = MeshToMeshFilterSets.AddDefaulted_GetRef();
							MeshToMeshFilterSet.SourceTriangles = MoveTemp(SimFaceSelection);
							MeshToMeshFilterSet.TargetVertices = MoveTemp(RenderVertexSelection);
						}
					}
				}
			}
		}

		return MeshToMeshFilterSets;
	}

}  // End namespace UE::Chaos::ClothAsset::Private

FChaosClothAssetProxyDeformerNode::FChaosClothAssetProxyDeformerNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	using namespace UE::Chaos::ClothAsset;
	SimVertexSelection.StringValue = FString();  // An empty selection is an accepted input, but a non existing one isn't
	SkinningBlendName = ClothCollectionAttribute::RenderDeformerSkinningBlend.ToString();

	RegisterInputConnection(&Collection);
	RegisterInputConnection(&SimVertexSelection.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelectionFilterSet0.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&SkinningBlendName);
}

void FChaosClothAssetProxyDeformerNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;

		// Update the selection names override
		SimVertexSelection.StringValue_Override = GetValue<FString>(Context, &SimVertexSelection.StringValue, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
		SelectionFilterSet0.StringValue_Override = GetValue<FString>(Context, &SelectionFilterSet0.StringValue, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
		const TArray<const FChaosClothAssetConnectableStringValue*> Non0SelectionFilterSets = Get1To9SelectionFilterSets();
		for (int32 FilterSetIndex = 1; FilterSetIndex < NumFilterSets; ++FilterSetIndex)
		{
			const FChaosClothAssetConnectableStringValue& SelectionFilterSet = *Non0SelectionFilterSets[FilterSetIndex - 1];
			SelectionFilterSet.StringValue_Override = GetValue<FString>(Context, &SelectionFilterSet.StringValue, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);
		}

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		// Always check for a valid cloth collection/facade to avoid processing non cloth collections
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (ClothFacade.IsValid() && ClothFacade.HasValidData())
		{
			FCollectionClothSelectionFacade SelectionFacade(ClothCollection);

			// Retrieve the SimVertexSelection name
			FName SimVertexSelectionName = FName(*GetValue<FString>(Context, &SimVertexSelection.StringValue));
			if (SimVertexSelectionName != NAME_None && (!SelectionFacade.IsValid() || !SelectionFacade.FindSelectionSet(SimVertexSelectionName)))
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("HasSimVertexSelectionHeadline", "Unknown SimVertexSelection."),
					LOCTEXT("HasSimVertexSelectionDetails", "The specified SimVertexSelection does't exist within the input Cloth Collection."));
				SimVertexSelectionName = NAME_None;
			}

			// Add the optional render deformer schema
			if (!ClothFacade.IsValid(EClothCollectionOptionalSchemas::RenderDeformer))
			{
				ClothFacade.DefineSchema(EClothCollectionOptionalSchemas::RenderDeformer);
			}

			// Create the render weight map for storing the skinning blend weights
			Private::FDeformerMappingDataGenerator DeformerMappingDataGenerator;
			DeformerMappingDataGenerator.SimPositions = ClothFacade.GetSimPosition3D();
			DeformerMappingDataGenerator.SimIndices = ClothFacade.GetSimIndices3D();
			DeformerMappingDataGenerator.RenderPositions = ClothFacade.GetRenderPosition();
			DeformerMappingDataGenerator.RenderNormals = ClothFacade.GetRenderNormal();
			DeformerMappingDataGenerator.RenderIndices = ClothFacade.GetRenderIndices();
			DeformerMappingDataGenerator.PointWeightMap = Private::SelectionToPointWeightMap(ClothFacade, SelectionFacade, SimVertexSelectionName);
			DeformerMappingDataGenerator.MeshToMeshFilterSet = Private::SelectionsToMeshToMeshFilterSets(ClothFacade, SelectionFacade, GetSelectionFilterNames(Context));
			DeformerMappingDataGenerator.RenderDeformerPositionBaryCoordsAndDist = ClothFacade.GetRenderDeformerPositionBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerNormalBaryCoordsAndDist = ClothFacade.GetRenderDeformerNormalBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerTangentBaryCoordsAndDist = ClothFacade.GetRenderDeformerTangentBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerSimIndices3D = ClothFacade.GetRenderDeformerSimIndices3D();
			DeformerMappingDataGenerator.RenderDeformerWeight = ClothFacade.GetRenderDeformerWeight();
			DeformerMappingDataGenerator.RenderDeformerSkinningBlend = ClothFacade.GetRenderDeformerSkinningBlend();

			const int32 NumInfluences = DeformerMappingDataGenerator.Generate(bUseSmoothTransition, bUseMultipleInfluences, InfluenceRadius);

			for (int32 RenderPatternIndex = 0; RenderPatternIndex < ClothFacade.GetNumRenderPatterns(); ++RenderPatternIndex)
			{
				FCollectionClothRenderPatternFacade RenderPatternFacade = ClothFacade.GetRenderPattern(RenderPatternIndex);
				RenderPatternFacade.SetRenderDeformerNumInfluences(NumInfluences);
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

Dataflow::FPin FChaosClothAssetProxyDeformerNode::AddPin()
{
	check(NumFilterSets > 0);
	const FChaosClothAssetConnectableStringValue* const SelectionFilterSet = Get1To9SelectionFilterSets()[NumFilterSets - 1];

	RegisterInputConnection(&SelectionFilterSet->StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	++NumFilterSets;
	const FDataflowInput* const Input = FindInput(SelectionFilterSet);
	check(Input);
	return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
}

Dataflow::FPin FChaosClothAssetProxyDeformerNode::GetPinToRemove() const
{
	check(NumFilterSets > 1);
	const FChaosClothAssetConnectableStringValue* const SelectionFilterSet = Get1To9SelectionFilterSets()[NumFilterSets - 2];
	const FDataflowInput* const Input = FindInput(SelectionFilterSet);
	check(Input);
	return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
}

void FChaosClothAssetProxyDeformerNode::OnPinRemoved(const Dataflow::FPin& Pin)
{
	check(NumFilterSets > 1);
	const FChaosClothAssetConnectableStringValue* const SelectionFilterSet = Get1To9SelectionFilterSets()[NumFilterSets - 2];
	check(Pin.Direction == Dataflow::FPin::EDirection::INPUT);
#if DO_CHECK
	const FDataflowInput* const Input = FindInput(SelectionFilterSet);
	check(Input);
	check(Input->GetName() == Pin.Name);
	check(Input->GetType() == Pin.Type);
#endif
	--NumFilterSets;
	return Super::OnPinRemoved(Pin);
}

void FChaosClothAssetProxyDeformerNode::Serialize(FArchive& Ar)
{
	// Restore the pins when re-loading so they can get properly reconnected
	if (Ar.IsLoading())
	{
		const int32 NumFilterSetsToAdd = (NumFilterSets - 1);
		NumFilterSets = 1;  // Reset to default, add pin will increment it again 
		for (int32 Index = 0; Index < NumFilterSetsToAdd; ++Index)
		{
			AddPin();
		}
		ensure(NumFilterSetsToAdd == (NumFilterSets - 1));
	}
}

TArray<FName> FChaosClothAssetProxyDeformerNode::GetSelectionFilterNames(Dataflow::FContext& Context) const
{
	check(NumFilterSets > 0);

	TArray<FName> SelectionFilterSets;
	SelectionFilterSets.SetNumUninitialized(NumFilterSets);

	SelectionFilterSets[0] = FName(*GetValue(Context, &SelectionFilterSet0.StringValue));

	TArray<const FChaosClothAssetConnectableStringValue*> Non0SelectionFilterSets = Get1To9SelectionFilterSets();

	for (int32 FilterSetIndex = 1; FilterSetIndex < NumFilterSets; ++FilterSetIndex)
	{
		SelectionFilterSets[FilterSetIndex] = FName(*GetValue(Context, &Non0SelectionFilterSets[FilterSetIndex - 1]->StringValue));
	}
	return SelectionFilterSets;
}

TArray<const FChaosClothAssetConnectableStringValue*> FChaosClothAssetProxyDeformerNode::Get1To9SelectionFilterSets() const
{
	return TArray<const FChaosClothAssetConnectableStringValue*>(
		{
			&SelectionFilterSet1,
			&SelectionFilterSet2,
			&SelectionFilterSet3,
			&SelectionFilterSet4,
			&SelectionFilterSet5,
			&SelectionFilterSet6,
			&SelectionFilterSet7,
			&SelectionFilterSet8,
			&SelectionFilterSet9
		});
}

#undef LOCTEXT_NAMESPACE
