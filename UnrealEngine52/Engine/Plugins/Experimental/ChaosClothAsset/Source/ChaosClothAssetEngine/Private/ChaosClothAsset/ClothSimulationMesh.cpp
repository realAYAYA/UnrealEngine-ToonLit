// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothSimulationMesh.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/ClothSimulationContext.h"
#include "ClothSimulationMesh.h"

namespace UE::Chaos::ClothAsset
{
	FClothSimulationMesh::FClothSimulationMesh(const FChaosClothSimulationModel& InClothSimulationModel, const FClothSimulationContext& InClothSimulationContext, const FString& DebugName)
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		: ::Chaos::FClothingSimulationMesh(DebugName)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		, ClothSimulationModel(InClothSimulationModel)
		, ClothSimulationContext(InClothSimulationContext)
	{
	}

	int32 FClothSimulationMesh::GetNumLODs() const
	{
		return ClothSimulationModel.GetNumLods();
	}

	int32 FClothSimulationMesh::GetLODIndex() const
	{
		return ClothSimulationContext.LodIndex;
	}

	int32 FClothSimulationMesh::GetOwnerLODIndex(int32 LODIndex) const
	{
		return LODIndex;  // The component LOD currently matches the asset LOD
	}

	bool FClothSimulationMesh::IsValidLODIndex(int32 LODIndex) const
	{
		return LODIndex >= 0 && LODIndex < GetNumLODs();
	}

	int32 FClothSimulationMesh::GetNumPoints(int32 LODIndex) const
	{
		return ClothSimulationModel.GetPositions(LODIndex).Num();
	}

	TConstArrayView<FVector3f> FClothSimulationMesh::GetPositions(int32 LODIndex) const
	{
		return TConstArrayView<FVector3f>(ClothSimulationModel.GetPositions(LODIndex));
	}

	TConstArrayView<FVector3f> FClothSimulationMesh::GetNormals(int32 LODIndex) const
	{
		return TConstArrayView<FVector3f>(ClothSimulationModel.GetNormals(LODIndex));
	}

	TConstArrayView<uint32> FClothSimulationMesh::GetIndices(int32 LODIndex) const
	{
		return TConstArrayView<uint32>(ClothSimulationModel.GetIndices(LODIndex));
	}

	TArray<TConstArrayView<::Chaos::FRealSingle>> FClothSimulationMesh::GetWeightMaps(int32 LODIndex) const
	{
		constexpr int32 MaxNumWeightMaps = 15; // TODO: Refactor how weight maps are used in base simulation classes

		TArray<TConstArrayView<::Chaos::FRealSingle>> WeightMaps;
		WeightMaps.SetNum(MaxNumWeightMaps);

		// Set max distance weight map
		constexpr int32 MaxDistanceWeightMapTarget = 1;		// EWeightMapTargetCommon::MaxDistance
		WeightMaps[MaxDistanceWeightMapTarget] = TConstArrayView<::Chaos::FRealSingle>(ClothSimulationModel.ClothSimulationLodModels[LODIndex].MaxDistance);

		return WeightMaps;
	}

	TArray<TConstArrayView<TTuple<int32, int32, float>>> FClothSimulationMesh::GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const
	{
		return TArray<TConstArrayView<TTuple<int32, int32, float>>>();  // TODO: Tethers
	}

	int32 FClothSimulationMesh::GetReferenceBoneIndex() const
	{
		return ClothSimulationModel.ReferenceBoneIndex;
	}

	FTransform FClothSimulationMesh::GetReferenceBoneTransform() const
	{
		// TODO: Leader pose component (see FClothingSimulationContextCommon::FillBoneTransforms)
		const TArray<FTransform>& BoneTransforms = ClothSimulationContext.BoneTransforms;

		if (BoneTransforms.IsValidIndex(ClothSimulationModel.ReferenceBoneIndex))
		{
			return BoneTransforms[ClothSimulationModel.ReferenceBoneIndex] * GetComponentToWorldTransform();
		}
		return GetComponentToWorldTransform();
	}

	const TArray<FTransform>& FClothSimulationMesh::GetBoneTransforms() const
	{
		// TODO: Leader pose component (see FClothingSimulationContextCommon::FillBoneTransforms)
		return ClothSimulationContext.BoneTransforms;
	}

	const FTransform& FClothSimulationMesh::GetComponentToWorldTransform() const
	{
		return ClothSimulationContext.ComponentTransform;
	}

	const TArray<FMatrix44f>& FClothSimulationMesh::GetRefToLocalMatrices() const
	{
		return ClothSimulationContext.RefToLocalMatrices;
	}

	TConstArrayView<int32> FClothSimulationMesh::GetBoneMap() const
	{
		return ClothSimulationModel.UsedBoneIndices;
	}

	TConstArrayView<FClothVertBoneData> FClothSimulationMesh::GetBoneData(int32 LODIndex) const
	{
		return ClothSimulationModel.GetBoneData(LODIndex);
	}

	TConstArrayView<FMeshToMeshVertData> FClothSimulationMesh::GetTransitionUpSkinData(int32 LODIndex) const
	{
		// TODO: UpSkinData
		return TConstArrayView<FMeshToMeshVertData>();
	}

	TConstArrayView<FMeshToMeshVertData> FClothSimulationMesh::GetTransitionDownSkinData(int32 LODIndex) const
	{
		// TODO: DownSkinData
		return TConstArrayView<FMeshToMeshVertData>();
	}
}
