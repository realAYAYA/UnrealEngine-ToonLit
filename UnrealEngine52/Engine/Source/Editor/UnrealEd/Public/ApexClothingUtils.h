// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"

class USkeletalMesh;

namespace ApexClothingUtils
{
	// Functions below remain from previous clothing system and only remain to remove the bound
	// data from a skeletal mesh. This is done when postloading USkeletalMesh when upgrading the assets

	// Function to restore all clothing section to original mesh section related to specified asset index
	UNREALED_API void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bReleaseAsset = true, bool bRecreateSkelMeshComponent = false);
}
