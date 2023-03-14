// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/AnimationImportTestFunctions.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimationImportTestFunctions)


UClass* UAnimationImportTestFunctions::GetAssociatedAssetType() const
{
	return USkeletalMesh::StaticClass();
}

