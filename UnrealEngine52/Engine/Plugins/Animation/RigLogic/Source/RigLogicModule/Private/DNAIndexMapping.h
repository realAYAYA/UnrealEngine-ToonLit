// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "Engine/AssetUserData.h"

#include "DNAIndexMapping.generated.h"

class IBehaviorReader;
class USkeleton;
class USkeletalMesh;
class USkeletalMeshComponent;

struct FDNAIndexMapping
{
	template <typename T>
	struct TArrayWrapper
	{
		TArray<T> Values;
	};

	FGuid SkeletonGuid;
	// For maps, we use int32 instead of SmartName::UID_Ttype directly to allow storing INDEX_NONE for missing elements
	// if value is valid, it is cast to SmartName::UID_Type
	TArray<int32> ControlAttributesMapDNAIndicesToUEUIDs;
	TArray<FMeshPoseBoneIndex> JointsMapDNAIndicesToMeshPoseBoneIndices;
	TArray<TArrayWrapper<int32>> BlendShapeIndicesPerLOD;
	TArray<TArrayWrapper<int32>> MorphTargetIndicesPerLOD;
	TArray<TArrayWrapper<int32>> AnimatedMapIndicesPerLOD;
	TArray<TArrayWrapper<int32>> MaskMultiplierIndicesPerLOD;

	void MapControlCurves(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton);
	void MapJoints(const IBehaviorReader* DNABehavior, const USkeletalMeshComponent* SkeletalMeshComponent);
	void MapMorphTargets(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton, const USkeletalMesh* SkeletalMesh);
	void MapMaskMultipliers(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton);

};

UCLASS(NotBlueprintable, hidecategories = (Object), deprecated)
class UDEPRECATED_DNAIndexMapping : public UAssetUserData
{
	GENERATED_BODY()
};