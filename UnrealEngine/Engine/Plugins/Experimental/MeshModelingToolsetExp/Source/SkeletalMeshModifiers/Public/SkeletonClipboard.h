// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/NameTypes.h"
#include "Math/MathFwd.h"

#include "SkeletonClipboard.generated.h"

class USkeletonModifier;

/**
 * The functions below are helper functions that simplify usage of copying/pasting bones on a FReferenceSkeleton
 * using a USkeletonModifier.
 */

namespace SkeletonClipboard
{
	/**
	 * Copies InBonesToCopy data (name, parent, transform) into the clipboard.
	 * Hierarchy info & transforms are retrieved via the InModifier.
	 */
	SKELETALMESHMODIFIERS_API void CopyToClipboard(const USkeletonModifier& InModifier, const TArray<FName>& InBonesToCopy);

	/**
	 * Paste new bones into the InOutModifier the clipboard InBonesToCopy data (name, parent, transform) into the clipboard.
	 * InDefaultParent is used as the default parent if a proper parent can't be retrieve from the clipboard data. 
	 */
	SKELETALMESHMODIFIERS_API TArray<FName> PasteFromClipboard(USkeletonModifier& InOutModifier, const FName& InDefaultParent);
	
	/**
	 * Checks whether the clipboard data is valid. 
	 */
	SKELETALMESHMODIFIERS_API bool IsClipboardValid();
}

USTRUCT()
struct FBoneClipboardData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName BoneName = NAME_None;

	UPROPERTY()
	FTransform Global = FTransform::Identity;

	UPROPERTY()
	int32 ParentIndex = INDEX_NONE;
};

USTRUCT()
struct FHierarchyClipboardData
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	TArray<FBoneClipboardData> Bones;
};

