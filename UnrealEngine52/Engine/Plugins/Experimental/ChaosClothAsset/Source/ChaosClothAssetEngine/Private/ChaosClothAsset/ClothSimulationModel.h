// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/MathFwd.h"
#include "Templates/SharedPointer.h"
#include "ClothVertBoneData.h"

#include "ClothSimulationModel.generated.h"

struct FReferenceSkeleton;
namespace UE::Chaos::ClothAsset
{
	class FClothCollection;
}

/**
 * Cloth simulation LOD model.
 * Contains the LOD data as fed to the solver for constraints creation.
 */
USTRUCT()
struct FChaosClothSimulationLodModel
{
	GENERATED_BODY()

	/** Vertex positions. */
	UPROPERTY()
	TArray<FVector3f> Positions;
	
	/** Vertex Normals. */
	UPROPERTY()
	TArray<FVector3f> Normals;

	/** Triangles indices. */
	UPROPERTY()
	TArray<uint32> Indices;

	/** Skinning weights. */
	UPROPERTY()
	TArray<FClothVertBoneData> BoneData;

	UPROPERTY()
	TArray<float> MaxDistance;
};

/**
 * Cloth simulation model.
 * Contains the LOD models.
 */
USTRUCT()
struct FChaosClothSimulationModel
{
	GENERATED_BODY()

	/** LOD data. */
	UPROPERTY()
	TArray<FChaosClothSimulationLodModel> ClothSimulationLodModels;

	/** List of bones this asset uses from the reference skeleton. */
	UPROPERTY()
	TArray<FName> UsedBoneNames;

	/** List of the indices for the bones in UsedBoneNames, used for remapping. */
	UPROPERTY()
	TArray<int32> UsedBoneIndices;

	/** Bone to treat as the root of the simulation space. */
	UPROPERTY()
	int32 ReferenceBoneIndex = INDEX_NONE;

	FChaosClothSimulationModel() = default;
	FChaosClothSimulationModel(const TSharedPtr<const UE::Chaos::ClothAsset::FClothCollection>& ClothCollection, const FReferenceSkeleton& ReferenceSkeleton);

	int32 GetNumLods() const { return ClothSimulationLodModels.Num(); }

	bool IsValidLodIndex(int32 LodIndex) const { return ClothSimulationLodModels.IsValidIndex(LodIndex); }

	int32 GetNumVertices(int32 LodIndex) const { return IsValidLodIndex(LodIndex) ? ClothSimulationLodModels[LodIndex].Positions.Num() : 0; }

	TConstArrayView<FVector3f> GetPositions(int32 LodIndex) const { return IsValidLodIndex(LodIndex) ? ClothSimulationLodModels[LodIndex].Positions : TConstArrayView<FVector3f>(); }
	TConstArrayView<FVector3f> GetNormals(int32 LodIndex) const { return IsValidLodIndex(LodIndex) ? ClothSimulationLodModels[LodIndex].Normals : TConstArrayView<FVector3f>(); }
	TConstArrayView<uint32> GetIndices(int32 LodIndex) const { return IsValidLodIndex(LodIndex) ? ClothSimulationLodModels[LodIndex].Indices : TConstArrayView<uint32>(); }
	TConstArrayView<FClothVertBoneData> GetBoneData(int32 LodIndex) const { return IsValidLodIndex(LodIndex) ? ClothSimulationLodModels[LodIndex].BoneData : TConstArrayView<FClothVertBoneData>(); }
};
