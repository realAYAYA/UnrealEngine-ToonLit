// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/AddWeightMapNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObject.h"
#include "Utils/ClothingMeshUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AddWeightMapNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetAddWeightMapNode"

namespace UE::Chaos::ClothAsset::Private
{
	void TransferWeightMap(
		const TConstArrayView<FVector3f>& SourcePositions,
		const TConstArrayView<FIntVector3>& InSourceIndices,
		const TConstArrayView<float>& SourceWeights,
		const TConstArrayView<FVector3f>& TargetPositions,
		const TConstArrayView<FVector3f>& TargetNormals,
		const TConstArrayView<FIntVector3>& InTargetIndices,
		TArray<float>& TargetWeights)
	{
		check(TargetWeights.Num() == TargetPositions.Num());
		if (!ensure(SourcePositions.Num() <= 65536))
		{
			return;  // MeshToMeshVertData below is limited to 16bit unsigned int indexes
		}

		TArray<uint32> SourceIndices;
		SourceIndices.Reserve(InSourceIndices.Num() * 3);
		for (const FIntVector3& InSourceIndex : InSourceIndices)
		{
			SourceIndices.Add(InSourceIndex[0]);
			SourceIndices.Add(InSourceIndex[1]);
			SourceIndices.Add(InSourceIndex[2]);
		}
		TArray<uint32> TargetIndices;
		TargetIndices.Reserve(InTargetIndices.Num() * 3);
		for (const FIntVector3& InTargetIndex : InTargetIndices)
		{
			TargetIndices.Add(InTargetIndex[0]);
			TargetIndices.Add(InTargetIndex[1]);
			TargetIndices.Add(InTargetIndex[2]);
		}

		const ClothingMeshUtils::ClothMeshDesc SourceMeshDesc(SourcePositions, SourceIndices);
		const ClothingMeshUtils::ClothMeshDesc TargetMeshDesc(TargetPositions, TargetNormals, TargetIndices);

		TArray<FMeshToMeshVertData> MeshToMeshVertData;
		const FPointWeightMap* const MaxDistances = nullptr; // No need to update the vertex contribution on the transition maps
		constexpr bool bUseSmoothTransitions = false;  // Smooth transitions are only used at rendering for now and not during LOD transitions
		constexpr bool bUseMultipleInfluences = false;  // Multiple influences must not be used for LOD transitions
		constexpr float SkinningKernelRadius = 0.f;  // KernelRadius is only required when using multiple influences

		ClothingMeshUtils::GenerateMeshToMeshVertData(
			MeshToMeshVertData,
			TargetMeshDesc,
			SourceMeshDesc,
			MaxDistances,
			bUseSmoothTransitions,
			bUseMultipleInfluences,
			SkinningKernelRadius);

		check(MeshToMeshVertData.Num() == TargetWeights.Num());
		for (int32 Index = 0; Index < TargetWeights.Num(); ++Index)
		{
			const FMeshToMeshVertData& MeshToMeshVertDatum = MeshToMeshVertData[Index];
			
			const uint16 VertIndex0 = MeshToMeshVertDatum.SourceMeshVertIndices[0];
			const uint16 VertIndex1 = MeshToMeshVertDatum.SourceMeshVertIndices[1];
			const uint16 VertIndex2 = MeshToMeshVertDatum.SourceMeshVertIndices[2];

			TargetWeights[Index] = FMath::Clamp(
				SourceWeights[VertIndex0] * MeshToMeshVertDatum.PositionBaryCoordsAndDist[0] +
				SourceWeights[VertIndex1] * MeshToMeshVertDatum.PositionBaryCoordsAndDist[1] +
				SourceWeights[VertIndex2] * MeshToMeshVertDatum.PositionBaryCoordsAndDist[2], 0.f, 1.f);
		}
	}

	void TransferWeightMap(
		const TConstArrayView<FVector2f>& InSourcePositions,
		const TConstArrayView<FIntVector3>& SourceIndices,
		const TConstArrayView<int32> SourceWeightsLookup,
		const TConstArrayView<float>& InSourceWeights,
		const TConstArrayView<FVector2f>& InTargetPositions,
		const TConstArrayView<FIntVector3>& TargetIndices,
		const TConstArrayView<int32> TargetWeightsLookup,
		TArray<float>& OutTargetWeights)
	{
		TArray<FVector3f> SourcePositions;
		SourcePositions.SetNumUninitialized(InSourcePositions.Num());
		for (int32 Index = 0; Index < SourcePositions.Num(); ++Index)
		{
			SourcePositions[Index] = FVector3f(InSourcePositions[Index], 0.f);
		}

		TArray<float> SourceWeights;
		SourceWeights.SetNumUninitialized(InSourcePositions.Num());
		for (int32 Index = 0; Index < SourceWeights.Num(); ++Index)
		{
			SourceWeights[Index] = InSourceWeights[SourceWeightsLookup[Index]];
		}

		TArray<FVector3f> TargetPositions;
		TArray<FVector3f> TargetNormals;
		TargetPositions.SetNumUninitialized(InTargetPositions.Num());
		TargetNormals.SetNumUninitialized(InTargetPositions.Num());
		for (int32 Index = 0; Index < TargetPositions.Num(); ++Index)
		{
			TargetPositions[Index] = FVector3f(InTargetPositions[Index], 0.f);
			TargetNormals[Index] = FVector3f::ZAxisVector;
		}

		TArray<float> TargetWeights;
		TargetWeights.SetNumUninitialized(TargetPositions.Num());

		TransferWeightMap(SourcePositions, SourceIndices, SourceWeights, TargetPositions, TargetNormals, TargetIndices, TargetWeights);

		for (int32 Index = 0; Index < TargetWeights.Num(); ++Index)
		{
			OutTargetWeights[TargetWeightsLookup[Index]] = TargetWeights[Index];
		}
	}
}

FChaosClothAssetAddWeightMapNode::FChaosClothAssetAddWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&TransferCollection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FChaosClothAssetAddWeightMapNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	using namespace UE::Chaos::ClothAsset;

	if (UChaosClothAsset* const ClothAsset = Cast<UChaosClothAsset>(Asset.Get()))
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: Remove after adding the getter function (currently shelved!)  
		if (UDataflow* const DataflowAsset = ClothAsset->DataflowAsset)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			const TSharedPtr<Dataflow::FGraph, ESPMode::ThreadSafe> Dataflow = DataflowAsset->GetDataflow();
			if (const TSharedPtr<FDataflowNode> BaseNode = Dataflow->FindBaseNode(this->GetGuid()))  // This is basically a safe const_cast
			{
				FChaosClothAssetAddWeightMapNode* const MutableThis = static_cast<FChaosClothAssetAddWeightMapNode*>(BaseNode.Get());
				check(MutableThis == this);

				// Make the name a valid attribute name, and replace the value in the UI
				FWeightMapTools::MakeWeightMapName(MutableThis->Name);

				// Transfer weight map if the transfer collection input has changed and is valid
				FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
				const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
				FCollectionClothConstFacade ClothFacade(ClothCollection);
				if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
				{
					FManagedArrayCollection InTransferCollection = GetValue<FManagedArrayCollection>(Context, &TransferCollection);
					const TSharedRef<const FManagedArrayCollection> TransferClothCollection = MakeShared<const FManagedArrayCollection>(MoveTemp(InTransferCollection));
					FCollectionClothConstFacade TransferClothFacade(TransferClothCollection);

					const FName InName(Name);
					const uint32 NameTypeHash = HashCombineFast(GetTypeHash(InName), (uint32)TransferType);
					const uint32 InTransferCollectionHash = (TransferClothFacade.IsValid() && InName != NAME_None) ?
						HashCombineFast(TransferClothFacade.CalculateWeightMapTypeHash(), NameTypeHash) : 0;  // TODO: Remove after adding the function (currently shelved!)  
				
					if (TransferCollectionHash != InTransferCollectionHash)
					{
						MutableThis->TransferCollectionHash = InTransferCollectionHash;

						if (TransferCollectionHash)
						{
							if (TransferClothFacade.HasWeightMap(InName))
							{
								// Remap the weights
								MutableThis->GetVertexWeights().SetNumZeroed(ClothFacade.GetNumSimVertices3D());

								switch (TransferType)
								{
								case EChaosClothAssetWeightMapTransferType::Use2DSimMesh:
									Private::TransferWeightMap(
										TransferClothFacade.GetSimPosition2D(),
										TransferClothFacade.GetSimIndices2D(),
										TransferClothFacade.GetSimVertex3DLookup(),
										TransferClothFacade.GetWeightMap(InName),
										ClothFacade.GetSimPosition2D(),
										ClothFacade.GetSimIndices2D(),
										ClothFacade.GetSimVertex3DLookup(),
										MutableThis->GetVertexWeights());
										break;
								case EChaosClothAssetWeightMapTransferType::Use3DSimMesh:
									Private::TransferWeightMap(
										TransferClothFacade.GetSimPosition3D(),
										TransferClothFacade.GetSimIndices3D(),
										TransferClothFacade.GetWeightMap(InName),
										ClothFacade.GetSimPosition3D(),
										ClothFacade.GetSimNormal(),
										ClothFacade.GetSimIndices3D(),
										MutableThis->GetVertexWeights());
										break;
								default: unimplemented();
								}
							}
						}
					}
				}
			}
		}
	}
}

void FChaosClothAssetAddWeightMapNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	auto CopyWeightsIntoClothCollection = [this](TArrayView<float>& ClothWeights, const TArray<float>& SourceVertexWeights, bool bIsSim)
	{
		const int32 MaxWeightIndex = FMath::Min(SourceVertexWeights.Num(), ClothWeights.Num());
		if (SourceVertexWeights.Num() > 0 && SourceVertexWeights.Num() != ClothWeights.Num())
		{
			FClothDataflowTools::LogAndToastWarning(*this,
				LOCTEXT("VertexCountMismatchHeadline", "Vertex count mismatch."),
				FText::Format(LOCTEXT("VertexCountMismatchDetails", "{0} vertex weights in the node: {1}\n{0} vertices in the cloth: {2}"),
					bIsSim ? FText::FromString("Sim") : FText::FromString("Render"),
					SourceVertexWeights.Num(),
					ClothWeights.Num()));
		}

		for (int32 VertexID = 0; VertexID < MaxWeightIndex; ++VertexID)
		{
			ClothWeights[VertexID] = SourceVertexWeights[VertexID];
		}
	};

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			const FName InName(Name);

			// Copy simulation weights into cloth collection

			if (MeshTarget == EChaosClothAssetWeightMapMeshType::Simulation || MeshTarget == EChaosClothAssetWeightMapMeshType::Both)
			{
				ClothFacade.AddWeightMap(InName);		// Does nothing if weight map already exists
				TArrayView<float> ClothSimWeights = ClothFacade.GetWeightMap(InName);

				if (ClothSimWeights.Num() != ClothFacade.GetNumSimVertices3D())
				{
					check(ClothSimWeights.Num() == 0);
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidSimWeightMapNameHeadline", "Invalid weight map name."),
						FText::Format(LOCTEXT("InvalidSimWeightMapNameDetails", "Could not create a sim weight map with name \"{0}\" (reserved name? wrong type?)."),
							FText::FromName(InName)));
				}
				else
				{
					constexpr bool bIsSim = true;
					CopyWeightsIntoClothCollection(ClothSimWeights, GetVertexWeights(), bIsSim);
				}
			}
			
			// Copy render weights into cloth collection

			if (MeshTarget == EChaosClothAssetWeightMapMeshType::Render || MeshTarget == EChaosClothAssetWeightMapMeshType::Both)
			{
				ClothFacade.AddUserDefinedAttribute<float>(InName, ClothCollectionGroup::RenderVertices);
				TArrayView<float> ClothRenderWeights = ClothFacade.GetUserDefinedAttribute<float>(InName, ClothCollectionGroup::RenderVertices);

				if (ClothRenderWeights.Num() != ClothFacade.GetNumRenderVertices())
				{
					check(ClothRenderWeights.Num() == 0);
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidRenderWeightMapNameHeadline", "Invalid weight map name."),
						FText::Format(LOCTEXT("InvalidRenderWeightMapNameDetails", "Could not create a render weight map with name \"{0}\" (reserved name? wrong type?)."),
							FText::FromName(InName)));
				}
				else
				{
					constexpr bool bIsSim = false;
					CopyWeightsIntoClothCollection(ClothRenderWeights, GetRenderVertexWeights(), bIsSim);
				}
			}

		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
	else if (Out->IsA<FString>(&Name))
	{
		SetValue(Context, Name, &Name);
	}
}

#undef LOCTEXT_NAMESPACE
