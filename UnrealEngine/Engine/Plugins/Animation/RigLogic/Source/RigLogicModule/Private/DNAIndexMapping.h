// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "Engine/AssetUserData.h"
#include "Animation/AnimCurveTypes.h"

#include "DNAIndexMapping.generated.h"

class IDNAReader;
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
	
	using FCachedIndexedCurve = TBaseBlendedCurve<FDefaultAllocator, UE::Anim::FCurveElementIndexed>; 

	FGuid SkeletonGuid;

	// all the control attributes that we will need to extract, alongside their control index
	FCachedIndexedCurve ControlAttributeCurves;
	FCachedIndexedCurve NeuralNetworkMaskCurves;
	TArray<FMeshPoseBoneIndex> JointsMapDNAIndicesToMeshPoseBoneIndices;
	TArray<FCachedIndexedCurve> MorphTargetCurvesPerLOD;
	TArray<FCachedIndexedCurve> MaskMultiplierCurvesPerLOD;

	void MapControlCurves(const IDNAReader* DNAReader, const USkeleton* Skeleton);
	void MapNeuralNetworkMaskCurves(const IDNAReader* DNAReader, const USkeleton* Skeleton);
	void MapJoints(const IDNAReader* DNAReader, const USkeletalMeshComponent* SkeletalMeshComponent);
	void MapMorphTargets(const IDNAReader* DNAReader, const USkeleton* Skeleton, const USkeletalMesh* SkeletalMesh);
	void MapMaskMultipliers(const IDNAReader* DNAReader, const USkeleton* Skeleton);

};

UCLASS(NotBlueprintable, hidecategories = (Object), deprecated)
class UDEPRECATED_DNAIndexMapping : public UAssetUserData
{
	GENERATED_BODY()
};