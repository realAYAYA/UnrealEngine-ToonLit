// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothLodTransitionDataCache.h"
#include "Utils/ClothingMeshUtils.h"
#include "ReferenceSkeleton.h"

namespace UE::Chaos::ClothAsset
{
	// Find the root bone for this cloth asset (common bone for all used bones)
	static int32 CalculateReferenceBoneIndex(const TArray<FChaosClothSimulationLodModel> ClothSimulationLodModels, const FReferenceSkeleton& ReferenceSkeleton, const TArray<int32>& UsedBoneIndices)
	{
		// Starts at root
		int32 ReferenceBoneIndex = 0;

		// List of valid paths to the root bone from each weighted bone
		TArray<TArray<int32>> PathsToRoot;

		// First build a list per used bone for it's path to root
		TArray<int32> WeightedBones;  // List of actually weighted (not just used) bones

		for (const FChaosClothSimulationLodModel& ClothSimulationLodModel : ClothSimulationLodModels)
		{
			for (const FClothVertBoneData& VertBoneData : ClothSimulationLodModel.BoneData)
			{
				for (int32 InfluenceIndex = 0; InfluenceIndex < VertBoneData.NumInfluences; ++InfluenceIndex)
				{
					if (VertBoneData.BoneWeights[InfluenceIndex] > SMALL_NUMBER)
					{
						const int32 UnmappedBoneIndex = VertBoneData.BoneIndices[InfluenceIndex];
						check(UsedBoneIndices.IsValidIndex(UnmappedBoneIndex));
						WeightedBones.AddUnique(UsedBoneIndices[UnmappedBoneIndex]);
					}
					else
					{
						// Hit the last weight (they're sorted)
						break;
					}
				}
			}
		}

		const int32 NumWeightedBones = WeightedBones.Num();
		PathsToRoot.Reserve(NumWeightedBones);

		// Compute paths to the root bone
		for (int32 WeightedBoneIndex = 0; WeightedBoneIndex < NumWeightedBones; ++WeightedBoneIndex)
		{
			PathsToRoot.AddDefaulted();
			TArray<int32>& Path = PathsToRoot.Last();

			int32 CurrentBone = WeightedBones[WeightedBoneIndex];
			Path.Add(CurrentBone);

			while (CurrentBone != 0 && CurrentBone != INDEX_NONE)
			{
				CurrentBone = ReferenceSkeleton.GetParentIndex(CurrentBone);
				Path.Add(CurrentBone);
			}
		}

		// Paths are from leaf->root, we want the other way
		for (TArray<int32>& Path : PathsToRoot)
		{
			Algo::Reverse(Path);
		}

		// Verify the last common bone in all paths as the root of the sim space
		const int32 NumPaths = PathsToRoot.Num();
		if (NumPaths > 0)
		{
			TArray<int32>& FirstPath = PathsToRoot[0];

			const int32 FirstPathSize = FirstPath.Num();
			for (int32 PathEntryIndex = 0; PathEntryIndex < FirstPathSize; ++PathEntryIndex)
			{
				const int32 CurrentQueryIndex = FirstPath[PathEntryIndex];
				bool bValidRoot = true;

				for (int32 PathIndex = 1; PathIndex < NumPaths; ++PathIndex)
				{
					if (!PathsToRoot[PathIndex].Contains(CurrentQueryIndex))
					{
						bValidRoot = false;
						break;
					}
				}

				if (bValidRoot)
				{
					ReferenceBoneIndex = CurrentQueryIndex;
				}
				else
				{
					// Once we fail to find a valid root we're done.
					break;
				}
			}
		}
		else
		{
			// Just use the root
			ReferenceBoneIndex = 0;
		}
		return ReferenceBoneIndex;
	}

	FMD5Hash CalculateTransitionDataHash(const FChaosClothSimulationLodModel& CurrModel)
	{
		FMD5 MD5;
		const TArray<FVector3f>& Positions = CurrModel.Positions;
		const int32 NumPositions = Positions.Num();
		MD5.Update(reinterpret_cast<const uint8*>(&NumPositions), sizeof(NumPositions));
		MD5.Update(reinterpret_cast<const uint8*>(Positions.GetData()),
			NumPositions * sizeof(FVector3f));

		const TArray<uint32>& Indices = CurrModel.Indices;
		const int32 NumIndices = Indices.Num();
		MD5.Update(reinterpret_cast<const uint8*>(&NumIndices), sizeof(NumIndices));
		MD5.Update(reinterpret_cast<const uint8*>(Indices.GetData()), NumIndices * sizeof(uint32));

		FMD5Hash MD5Hash;
		MD5Hash.Set(MD5);

		return MD5Hash;
	}
}  // End namespace UE::Chaos::ClothAsset

bool FChaosClothSimulationLodModel::Serialize(FArchive& Ar)
{
	// Serialize normal tagged property data
	if (Ar.IsLoading() || Ar.IsSaving())
	{
		UScriptStruct* const Struct = FChaosClothSimulationLodModel::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}

	// Serialize weight maps (not a tagged property)
	Ar << WeightMaps;

	Ar << LODTransitionUpData;
	Ar << LODTransitionDownData;

	// Return true to confirm that serialization has already been taken care of
	return true;
}

FChaosClothSimulationModel::FChaosClothSimulationModel(const TArray<TSharedRef<const FManagedArrayCollection>>& ClothCollections, 
	const FReferenceSkeleton& ReferenceSkeleton,
	TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache)
{
	using namespace UE::Chaos::ClothAsset;

	// Initialize LOD models
	const int32 NumLods = ClothCollections.Num();
	ClothSimulationLodModels.SetNum(NumLods);

	for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
	{
		const FCollectionClothConstFacade Cloth(ClothCollections[LodIndex]);

		// Retrieve weigh map names
		const TArray<FName> WeightMapNames = Cloth.GetWeightMapNames();

		FChaosClothSimulationLodModel& LodModel = ClothSimulationLodModels[LodIndex];

		TArray<TArray<int32>> WeldedToPatternIndices;
		Cloth.BuildSimulationMesh(LodModel.Positions, LodModel.Normals, LodModel.Indices, LodModel.PatternPositions, LodModel.PatternIndices, LodModel.PatternToWeldedIndices, &WeldedToPatternIndices);

		// Copy weight maps
		LodModel.WeightMaps.Reserve(WeightMapNames.Num());

		for (const FName& WeightMapName : WeightMapNames)
		{
			LodModel.WeightMaps.Add(WeightMapName) = Cloth.GetWeightMap(WeightMapName);
		}

		// Copy bone influences and gather tether data
		const int32 NumSimVertices3D = Cloth.GetNumSimVertices3D();
		TConstArrayView<TArray<int32>> SimBoneIndices = Cloth.GetSimBoneIndices();
		TConstArrayView<TArray<float>> SimBoneWeights = Cloth.GetSimBoneWeights();
		TConstArrayView<TArray<int32>> TetherKinematicIndex = Cloth.GetTetherKinematicIndex();
		TConstArrayView<TArray<float>> TetherReferenceLength = Cloth.GetTetherReferenceLength();
		TArray<TArray<TPair<float, int32>>> MergedTetherData;
		LodModel.BoneData.SetNum(NumSimVertices3D);
		MergedTetherData.SetNum(NumSimVertices3D);

		for (int32 VertexIndex = 0; VertexIndex < NumSimVertices3D; ++VertexIndex)
		{
			check(SimBoneIndices[VertexIndex].Num() == SimBoneWeights[VertexIndex].Num());
			check(SimBoneIndices[VertexIndex].Num() <= FClothVertBoneData::MaxTotalInfluences);
			LodModel.BoneData[VertexIndex].NumInfluences = SimBoneIndices[VertexIndex].Num();
			for (int32 BoneIndex = 0; BoneIndex < LodModel.BoneData[VertexIndex].NumInfluences; ++BoneIndex)
			{
				LodModel.BoneData[VertexIndex].BoneIndices[BoneIndex] = SimBoneIndices[VertexIndex][BoneIndex];
				LodModel.BoneData[VertexIndex].BoneWeights[BoneIndex] = SimBoneWeights[VertexIndex][BoneIndex];
			}

			check(TetherKinematicIndex[VertexIndex].Num() == TetherReferenceLength[VertexIndex].Num());
			MergedTetherData[VertexIndex].Reserve(TetherKinematicIndex[VertexIndex].Num());
			for (int32 TetherIdx = 0; TetherIdx < TetherKinematicIndex[VertexIndex].Num(); ++TetherIdx)
			{
				MergedTetherData[VertexIndex].Emplace(TetherReferenceLength[VertexIndex][TetherIdx], TetherKinematicIndex[VertexIndex][TetherIdx]);
			}
		}

		// Batch tethers
		LodModel.TetherData.GenerateTethers(MoveTemp(MergedTetherData));
	}

	// Populate used bone names and indices
	for (int32 Index = 0; Index < ReferenceSkeleton.GetRawBoneNum(); ++Index)
	{	
		UsedBoneNames.Add(ReferenceSkeleton.GetRawRefBoneInfo()[Index].Name);
		UsedBoneIndices.Add(Index);
	}

	// Initialize Reference bone index
	ReferenceBoneIndex = CalculateReferenceBoneIndex(ClothSimulationLodModels, ReferenceSkeleton, UsedBoneIndices);

	CalculateLODTransitionUpDownData(InOutTransitionCache);
}

TArray<TConstArrayView<TTuple<int32, int32, float>>> FChaosClothSimulationModel::GetTethers(int32 LODIndex) const
{
	TArray<TConstArrayView<TTuple<int32, int32, float>>> Tethers;
	if (IsValidLodIndex(LODIndex))
	{
		const FClothTetherData& ClothTetherData = ClothSimulationLodModels[LODIndex].TetherData;

		const int32 NumTetherBatches = ClothTetherData.Tethers.Num();
		Tethers.Reserve(NumTetherBatches);
		for (int32 Index = 0; Index < NumTetherBatches; ++Index)
		{
			Tethers.Emplace(TConstArrayView<TTuple<int32, int32, float>>(ClothTetherData.Tethers[Index]));
		}
	}
	return Tethers;
}

void FChaosClothSimulationModel::CalculateLODTransitionUpDownData(TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache)
{
	// Parameters for GenerateMeshToMeshVertData
	const FPointWeightMap* const MaxDistances = nullptr;  // No need to update the vertex contribution on the transition maps
	constexpr bool bUseSmoothTransitions = false;         // Smooth transitions are only used at rendering for now and not during LOD transitions
	constexpr bool bUseMultipleInfluences = false;        // Multiple influences must not be used for LOD transitions
	constexpr float SkinningKernelRadius = 0.f;           // KernelRadius is only required when using multiple influences

	// Update cache hashes.
	TArray<bool> TransitionCacheHashValid;
	TransitionCacheHashValid.SetNumZeroed(ClothSimulationLodModels.Num());
	if (InOutTransitionCache)
	{
		InOutTransitionCache->SetNum(ClothSimulationLodModels.Num());

		for (int32 LODIndex = 0; LODIndex < ClothSimulationLodModels.Num(); ++LODIndex)
		{
			const FMD5Hash CurrCacheHash = UE::Chaos::ClothAsset::CalculateTransitionDataHash(ClothSimulationLodModels[LODIndex]);
			TransitionCacheHashValid[LODIndex] = (CurrCacheHash == (*InOutTransitionCache)[LODIndex].ModelHash);
			(*InOutTransitionCache)[LODIndex].ModelHash = CurrCacheHash;
		}
	}

	for (int32 LODIndex = 0; LODIndex < ClothSimulationLodModels.Num(); ++LODIndex)
	{
		const int32 PrevLODIndex = LODIndex - 1;
		const int32 NextLODIndex = LODIndex + 1;
				
		FChaosClothSimulationLodModel& CurrModel = ClothSimulationLodModels[LODIndex];
		const ClothingMeshUtils::ClothMeshDesc CurrentMeshDesc(CurrModel.Positions, CurrModel.Indices);

		if (ClothSimulationLodModels.IsValidIndex(PrevLODIndex))
		{
			if (TransitionCacheHashValid[LODIndex] && TransitionCacheHashValid[PrevLODIndex])
			{
				CurrModel.LODTransitionUpData = (*InOutTransitionCache)[LODIndex].LODTransitionUpData;
			}
			else
			{
				const FChaosClothSimulationLodModel& PrevModel = ClothSimulationLodModels[PrevLODIndex];
				const ClothingMeshUtils::ClothMeshDesc PrevMeshDesc(PrevModel.Positions, PrevModel.Indices);

				ClothingMeshUtils::GenerateMeshToMeshVertData(CurrModel.LODTransitionUpData, CurrentMeshDesc, PrevMeshDesc,
					MaxDistances, bUseSmoothTransitions, bUseMultipleInfluences, SkinningKernelRadius);

				if (InOutTransitionCache)
				{
					(*InOutTransitionCache)[LODIndex].LODTransitionUpData = CurrModel.LODTransitionUpData;
				}
			}
		}

		if (ClothSimulationLodModels.IsValidIndex(NextLODIndex))
		{
			if (TransitionCacheHashValid[LODIndex] && TransitionCacheHashValid[NextLODIndex])
			{
				CurrModel.LODTransitionDownData = (*InOutTransitionCache)[LODIndex].LODTransitionDownData;
			}
			else
			{
				const FChaosClothSimulationLodModel& NextModel = ClothSimulationLodModels[NextLODIndex];
				const ClothingMeshUtils::ClothMeshDesc NextMeshDesc(NextModel.Positions, NextModel.Indices);

				ClothingMeshUtils::GenerateMeshToMeshVertData(CurrModel.LODTransitionDownData, CurrentMeshDesc, NextMeshDesc,
					MaxDistances, bUseSmoothTransitions, bUseMultipleInfluences, SkinningKernelRadius);

				if (InOutTransitionCache)
				{
					(*InOutTransitionCache)[LODIndex].LODTransitionDownData = CurrModel.LODTransitionDownData;
				}
			}
		}
	}
}