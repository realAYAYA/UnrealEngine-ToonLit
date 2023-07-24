// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulationSkeletalMesh.h"
#include "ChaosCloth/ChaosClothPrivate.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosWeightMapTarget.h"
#include "ClothingAsset.h"
#include "ClothingSimulation.h"

namespace Chaos
{
	FClothingSimulationSkeletalMesh::FClothingSimulationSkeletalMesh(const UClothingAssetCommon* InAsset, const USkeletalMeshComponent* InSkeletalMeshComponent)
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		: FClothingSimulationMesh(InSkeletalMeshComponent->GetOwner() ?
				FString::Format(TEXT("{0}|{1}"), { InSkeletalMeshComponent->GetOwner()->GetName(), InSkeletalMeshComponent->GetName() }) :
				InSkeletalMeshComponent->GetName())
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		, Asset(InAsset)
		, SkeletalMeshComponent(InSkeletalMeshComponent)
	{
	}

	int32 FClothingSimulationSkeletalMesh::GetNumLODs() const
	{
		return Asset ? Asset->LodData.Num() : 0;
	}

	int32 FClothingSimulationSkeletalMesh::GetLODIndex() const
	{
		int32 LODIndex = INDEX_NONE;

		if (Asset && SkeletalMeshComponent)
		{
			const int32 OwnerLODIndex = SkeletalMeshComponent->GetPredictedLODLevel();
			if (Asset->LodMap.IsValidIndex(OwnerLODIndex))
			{
				const int32 ClothLODIndex = Asset->LodMap[OwnerLODIndex];
				if (Asset->LodData.IsValidIndex(ClothLODIndex))
				{
					LODIndex = ClothLODIndex;
				}
			}
		}
		return LODIndex;
	}

	int32 FClothingSimulationSkeletalMesh::GetOwnerLODIndex(int32 LODIndex) const
	{
		const int32 OwnerLODIndex = Asset ? Asset->LodMap.Find(LODIndex) : INDEX_NONE;
		return OwnerLODIndex != INDEX_NONE ? OwnerLODIndex : 0;  // Safer to return the default LOD 0 than INDEX_NONE in this case
	}

	bool FClothingSimulationSkeletalMesh::IsValidLODIndex(int32 LODIndex) const
	{
		return Asset && Asset->LodData.IsValidIndex(LODIndex);
	}

	int32 FClothingSimulationSkeletalMesh::GetNumPoints(int32 LODIndex) const
	{
		return IsValidLODIndex(LODIndex) ? Asset->LodData[LODIndex].PhysicalMeshData.Vertices.Num() : 0;
	}

	TConstArrayView<FVector3f> FClothingSimulationSkeletalMesh::GetPositions(int32 LODIndex) const
	{
		if (IsValidLODIndex(LODIndex))
		{
			const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
			const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;
			return ClothPhysicalMeshData.Vertices;
		}
		return TConstArrayView<FVector3f>();
	}

	TConstArrayView<FVector3f> FClothingSimulationSkeletalMesh::GetNormals(int32 LODIndex) const
	{
		if (IsValidLODIndex(LODIndex))
		{
			const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
			const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;
			return ClothPhysicalMeshData.Normals;
		}
		return TConstArrayView<FVector3f>();
	}

	TConstArrayView<uint32> FClothingSimulationSkeletalMesh::GetIndices(int32 LODIndex) const
	{
		return IsValidLODIndex(LODIndex) ?
			TConstArrayView<uint32>(Asset->LodData[LODIndex].PhysicalMeshData.Indices) :
			TConstArrayView<uint32>();
	}

	TArray<TConstArrayView<FRealSingle>> FClothingSimulationSkeletalMesh::GetWeightMaps(int32 LODIndex) const
	{
		TArray<TConstArrayView<FRealSingle>> WeightMaps;
		if (IsValidLODIndex(LODIndex))
		{
			const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
			const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;

			const UEnum* const ChaosWeightMapTargetEnum = StaticEnum<EChaosWeightMapTarget>();
			const int32 NumWeightMaps = (int32)ChaosWeightMapTargetEnum->GetMaxEnumValue() + 1;

			WeightMaps.SetNum(NumWeightMaps);

			for (int32 EnumIndex = 0; EnumIndex < ChaosWeightMapTargetEnum->NumEnums(); ++EnumIndex)
			{
				const int32 TargetIndex = (int32)ChaosWeightMapTargetEnum->GetValueByIndex(EnumIndex);
				if (const FPointWeightMap* const WeightMap = ClothPhysicalMeshData.FindWeightMap(TargetIndex))
				{
					WeightMaps[TargetIndex] = WeightMap->Values;
				}
			}
		}
		return WeightMaps;
	}

	TArray<TConstArrayView<TTuple<int32, int32, float>>> FClothingSimulationSkeletalMesh::GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const
	{
		TArray<TConstArrayView<TTuple<int32, int32, float>>> Tethers;
		if (IsValidLODIndex(LODIndex))
		{
			const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
			const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;
			const FClothTetherData& ClothTetherData = bUseGeodesicTethers ? ClothPhysicalMeshData.GeodesicTethers : ClothPhysicalMeshData.EuclideanTethers;
		
			const int32 NumTetherBatches = ClothTetherData.Tethers.Num();
			Tethers.Reserve(NumTetherBatches);
			for (int32 Index = 0; Index < NumTetherBatches; ++Index)
			{
				Tethers.Emplace(TConstArrayView<TTuple<int32, int32, float>>(ClothTetherData.Tethers[Index]));
			}
		}
		return Tethers;
	}

	int32 FClothingSimulationSkeletalMesh::GetReferenceBoneIndex() const
	{
		return Asset ? Asset->ReferenceBoneIndex : INDEX_NONE;
	}

	FTransform FClothingSimulationSkeletalMesh::GetReferenceBoneTransform() const
	{
		if (const FClothingSimulationContextCommon* const Context = GetContext())
		{
			const int32 ReferenceBoneIndex = GetReferenceBoneIndex();
			const TArray<FTransform>& BoneTransforms = Context->BoneTransforms;

			return BoneTransforms.IsValidIndex(ReferenceBoneIndex) ?
				BoneTransforms[ReferenceBoneIndex] * Context->ComponentToWorld :
				Context->ComponentToWorld;
		}
		return FTransform::Identity;
	}

	const TArray<FTransform>& FClothingSimulationSkeletalMesh::GetBoneTransforms() const
	{
		return GetContext()->BoneTransforms;
	}

	const FTransform& FClothingSimulationSkeletalMesh::GetComponentToWorldTransform() const
	{
		return GetContext() ? GetContext()->ComponentToWorld : FTransform::Identity;
	}

	const TArray<FMatrix44f>& FClothingSimulationSkeletalMesh::GetRefToLocalMatrices() const
	{
		static TArray<FMatrix44f> EmptyArray;
		return GetContext() ? GetContext()->RefToLocals : EmptyArray;
	}

	TConstArrayView<int32> FClothingSimulationSkeletalMesh::GetBoneMap() const
	{
		return Asset ? Asset->UsedBoneIndices : TConstArrayView<int32>();
	}

	TConstArrayView<FClothVertBoneData> FClothingSimulationSkeletalMesh::GetBoneData(int32 LODIndex) const
	{
		if (IsValidLODIndex(LODIndex))
		{
			const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
			const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;
			UE_CLOG(ClothPhysicalMeshData.MaxBoneWeights > 12, LogChaosCloth, Warning, TEXT("The cloth physics mesh skinning code can't cope with more than 12 bone influences."));
			return ClothPhysicalMeshData.BoneData;
		}
		return TConstArrayView<FClothVertBoneData>();
	}

	TConstArrayView<FMeshToMeshVertData> FClothingSimulationSkeletalMesh::GetTransitionUpSkinData(int32 LODIndex) const
	{
		return IsValidLODIndex(LODIndex) ? TConstArrayView<FMeshToMeshVertData>(Asset->LodData[LODIndex].TransitionUpSkinData) : TConstArrayView<FMeshToMeshVertData>();
	}

	TConstArrayView<FMeshToMeshVertData> FClothingSimulationSkeletalMesh::GetTransitionDownSkinData(int32 LODIndex) const
	{
		return IsValidLODIndex(LODIndex) ? TConstArrayView<FMeshToMeshVertData>(Asset->LodData[LODIndex].TransitionDownSkinData) : TConstArrayView<FMeshToMeshVertData>();
	}

	const FClothingSimulationContextCommon* FClothingSimulationSkeletalMesh::GetContext() const
	{
		return GetSkeletalMeshComponent() ?
			static_cast<const FClothingSimulationContextCommon*>(GetSkeletalMeshComponent()->GetClothingSimulationContext()) :
			nullptr;
	}
}  // End namespace Chaos
