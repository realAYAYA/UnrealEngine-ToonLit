// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GroomSettings.h"
#include "HairStrandsInterface.h"
#include "HairStrandsMeshProjection.h"
#include "HairStrandsDatas.h"

struct FHairStrandsRestRootResource;

struct HAIRSTRANDSCORE_API FGroomBindingBuilder
{
	static FString GetVersion();

	// Build binding asset data
	static bool BuildBinding(class UGroomBindingAsset* BindingAsset, bool bInitResources);

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