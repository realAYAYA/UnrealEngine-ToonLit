// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "ChaosClothAsset/ClothAsset.h"

class FSkeletalMeshLODModel;

/** Builder utility struct, nested struct of UChaosClothAsset. */
struct UChaosClothAsset::FBuilder
{
	/** Build a FSkeletalMeshLODModel out of the cloth asset for the specified LOD index and specified platform. */
	static void BuildLod(FSkeletalMeshLODModel& LODModel, const UChaosClothAsset& ClothAsset, int32 LodIndex, const ITargetPlatform* TargetPlatform);
};

#endif  // #if WITH_EDITOR
