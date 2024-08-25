// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GroomSettings.h"
#include "HairStrandsInterface.h"
#include "HairStrandsMeshProjection.h"
#include "HairStrandsDatas.h"
#include "GroomBindingAsset.h"

class ITargetPlatform;

struct HAIRSTRANDSCORE_API FGroomBindingBuilder
{
	struct FInput
	{
		EGroomBindingMeshType BindingType = EGroomBindingMeshType::SkeletalMesh;
		int32 NumInterpolationPoints = 0;
		int32 MatchingSection = 0;
		bool bHasValidTarget = false;
		UGroomAsset* GroomAsset = nullptr;
		USkeletalMesh* SourceSkeletalMesh = nullptr;
		USkeletalMesh* TargetSkeletalMesh = nullptr;
		UGeometryCache* SourceGeometryCache;
		UGeometryCache* TargetGeometryCache;
	};

	static FString GetVersion();

	// Build binding asset data
	UE_DEPRECATED(5.4, "Please do not access this funciton; but rather call BindingAsset->CacheDerivedDatas()")
	static bool BuildBinding(class UGroomBindingAsset* BindingAsset, bool bInitResource);

	// Build binding asset data
	UE_DEPRECATED(5.4, "Please do not access this funciton; but rather call BindingAsset->CacheDerivedDatas()")
	static bool BuildBinding(class UGroomBindingAsset* BindingAsset, uint32 InGroupIndex);

	// Build binding asset data
	static bool BuildBinding(const FInput& In, uint32 InGroupIndex, const ITargetPlatform* TargetPlatform, UGroomBindingAsset::FHairGroupPlatformData& Out);

	// Extract root data from bulk data
	static void GetRootData(FHairStrandsRootData& Out, const FHairStrandsRootBulkData& In);
};

namespace GroomBinding_RBFWeighting
{
	struct FPointsSampler
	{
		FPointsSampler(TArray<bool>& ValidPoints, const FVector3f* PointPositions, const int32 NumSamples);

		/** Build the sample position from the sample indices */
		void BuildPositions(const FVector3f* PointPositions);

		/** Compute the furthest point */
		void FurthestPoint(const int32 NumPoints, const FVector3f* PointPositions, const uint32 SampleIndex, TArray<bool>& ValidPoints, TArray<float>& PointsDistance);

		/** Compute the starting point */
		int32 StartingPoint(const TArray<bool>& ValidPoints, int32& NumPoints) const;

		/** List of sampled points */
		TArray<uint32> SampleIndices;

		/** List of sampled positions */
		TArray<FVector3f> SamplePositions;
	};
}