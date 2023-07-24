// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ClothAssetBuilder.h"

#include "ClothAssetBuilderEditor.generated.h"

class FSkeletalMeshLODModel;

/**
 * Implement a clothing asset builder.
 */
UCLASS()
class UClothAssetBuilderEditor : public UClothAssetBuilder
{
	GENERATED_BODY()

public:
	/** Build a FSkeletalMeshLODModel out of the cloth asset for the specified LOD index. */
	virtual void BuildLod(FSkeletalMeshLODModel& LODModel, const UChaosClothAsset& ClothAsset, int32 LodIndex) const override;
};
