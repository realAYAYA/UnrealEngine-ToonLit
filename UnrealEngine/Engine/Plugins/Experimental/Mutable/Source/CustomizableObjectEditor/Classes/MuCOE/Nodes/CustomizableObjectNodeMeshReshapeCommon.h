// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

#include "CustomizableObjectNodeMeshReshapeCommon.generated.h"

UENUM()
enum class EBoneDeformSelectionMethod : uint8
{
	// Only selected bones will be deform
	ONLY_SELECTED = 0 UMETA(DisplayName = "Only Selected"),

	// All bones will be deform except the selected ones
	ALL_BUT_SELECTED = 1 UMETA(DisplayName = "All But Selected"),

	// Deform only the bones of the reference skeleton
	DEFORM_REF_SKELETON = 2 UMETA(DisplayName = "Deform Only Ref.Seleton Bones"),

	// Deform only the bones that are not in the reference skeleton
	DEFORM_NONE_REF_SKELETON = 3 UMETA(DisplayName = "Deform All But not Ref.Skeleton Bones")
};


USTRUCT()
struct FMeshReshapeBoneReference
{
	GENERATED_USTRUCT_BODY()

	/** Name of the bone that will be deformed */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FName BoneName;
};
