// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UGroomAsset;
class USkeletalMesh;

struct HAIRSTRANDSCORE_API FGroomDeformerBuilder
{
	// Create skeletal mesh bones+physics asset from groom asset guides
	static USkeletalMesh* CreateSkeletalMesh(UGroomAsset* GroomAsset);
};
