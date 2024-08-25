// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosCloth/ChaosClothingSimulationMesh.h"

class FClothingSimulationContextCommon;
class UClothingAssetCommon;

namespace Chaos
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	class FClothingSimulationSkeletalMesh final : public FClothingSimulationMesh
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
	public:
		FClothingSimulationSkeletalMesh(const UClothingAssetCommon* InAsset, const USkeletalMeshComponent* InSkeletalMeshComponent);
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		virtual ~FClothingSimulationSkeletalMesh() override = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FClothingSimulationSkeletalMesh(const FClothingSimulationSkeletalMesh&) = delete;
		FClothingSimulationSkeletalMesh(FClothingSimulationSkeletalMesh&&) = delete;
		FClothingSimulationSkeletalMesh& operator=(const FClothingSimulationSkeletalMesh&) = delete;
		FClothingSimulationSkeletalMesh& operator=(FClothingSimulationSkeletalMesh&&) = delete;

#if !defined(CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT) || !CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		virtual const UClothingAssetCommon* GetAsset() const override { return Asset; }
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		virtual const USkeletalMeshComponent* GetSkeletalMeshComponent() const override { return SkeletalMeshComponent; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
		const UClothingAssetCommon* GetAsset() const { return Asset; }
		const USkeletalMeshComponent* GetSkeletalMeshComponent() const { return Cast<USkeletalMeshComponent>(GetSkinnedMeshComponent()); }
#endif
		//~ Begin FClothingSimulationMesh Interface
		virtual int32 GetNumLODs() const override;
		virtual int32 GetLODIndex() const override;
		virtual int32 GetOwnerLODIndex(int32 LODIndex) const override;
		virtual bool IsValidLODIndex(int32 LODIndex) const override;
		virtual int32 GetNumPoints(int32 LODIndex) const override;
		virtual int32 GetNumPatternPoints(int32 LODIndex) const override;
		virtual TConstArrayView<FVector3f> GetPositions(int32 LODIndex) const override;
		virtual TConstArrayView<FVector2f> GetPatternPositions(int32 LODIndex) const override;
		virtual TConstArrayView<FVector3f> GetNormals(int32 LODIndex) const override;
		virtual TConstArrayView<uint32> GetIndices(int32 LODIndex) const override;
		virtual TConstArrayView<uint32> GetPatternIndices(int32 LODIndex) const override;
		virtual TConstArrayView<uint32> GetPatternToWeldedIndices(int32 LODIndex) const override;
		virtual TArray<FName> GetWeightMapNames(int32 LODIndex) const override;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TArray<FName> GetWeightMapNames() const override { return GetWeightMapNames(0); }
		virtual TMap<FString, int32> GetWeightMapIndices(int32 LODIndex) const override;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TMap<FString, int32> GetWeightMapIndices() const override { return GetWeightMapIndices(0); }
		virtual TArray<TConstArrayView<FRealSingle>> GetWeightMaps(int32 LODIndex) const override;
		virtual TMap<FString, const TSet<int32>*> GetVertexSets(int32 LODIndex) const override;
		virtual TMap<FString, const TSet<int32>*> GetFaceSets(int32 LODIndex) const override;
		virtual TMap<FString, TConstArrayView<int32>> GetFaceIntMaps(int32 LODIndex) const override;
		virtual TArray<TConstArrayView<TTuple<int32, int32, float>>> GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const override;
		virtual int32 GetReferenceBoneIndex() const override;
		virtual FTransform GetReferenceBoneTransform() const override;
		virtual const TArray<FTransform>& GetBoneTransforms() const override;
		virtual const FTransform& GetComponentToWorldTransform() const override;
		virtual const TArray<FMatrix44f>& GetRefToLocalMatrices() const override;
		virtual TConstArrayView<int32> GetBoneMap() const override;
		virtual TConstArrayView<FClothVertBoneData> GetBoneData(int32 LODIndex) const override;
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionUpSkinData(int32 LODIndex) const override;
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionDownSkinData(int32 LODIndex) const override;
		//~ End FClothingSimulationMesh Interface

	private:
		const FClothingSimulationContextCommon* GetContext() const;

		const UClothingAssetCommon* Asset;
		const USkeletalMeshComponent* SkeletalMeshComponent;
	};
}
