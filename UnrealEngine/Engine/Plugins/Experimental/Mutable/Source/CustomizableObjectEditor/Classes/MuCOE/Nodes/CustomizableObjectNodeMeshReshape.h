// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshReshape.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FMeshReshapeBoneReference;
enum class EBoneDeformSelectionMethod : uint8;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshReshape : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshReshape();

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	FString GetRefreshMessage() const override;

	inline UEdGraphPin* BaseMeshPin() const
	{
		return FindPin(TEXT("Base Mesh"), EGPD_Input);
	}

	inline UEdGraphPin* BaseShapePin() const
	{
		return FindPin(TEXT("Base Shape"), EGPD_Input);
	}

	inline UEdGraphPin* TargetShapePin() const
	{
		return FindPin(TEXT("Target Shape"), EGPD_Input);
	}

	void Serialize(FArchive& Ar) override;

	/** Enable the deformation of the skeleton of the base mesh. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReshapeSkeleton = false;	

	/** Enable the deformation of physics volumes of the base mesh */
    UPROPERTY(EditAnywhere, Category = CustomizableObject)
    bool bReshapePhysicsVolumes = false;
	
	/** Enable rigid parts base mesh. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bEnableRigidParts = false;

	/** Bone Reshape Selection Method */
	UPROPERTY(EditAnywhere, Category = ReshapeBones, meta = (EditCondition = "bReshapeSkeleton"))
	EBoneDeformSelectionMethod SelectionMethod;

	/** Enables the deformation of all bones of the skeleton */
	UPROPERTY()
	bool bDeformAllBones_DEPRECATED = false;

	/** Array with selected bones that will be deformed */
	UPROPERTY(EditAnywhere, Category = ReshapeBones, meta = (EditCondition = "bReshapeSkeleton && (SelectionMethod == EBoneDeformSelectionMethod::ONLY_SELECTED || SelectionMethod == EBoneDeformSelectionMethod::ALL_BUT_SELECTED)"))
	TArray<FMeshReshapeBoneReference> BonesToDeform;

	/** Enables the deformation of all physic bodies*/
	UPROPERTY(EditAnywhere, Category = ReshapePhysics, meta = (EditCondition = "bReshapePhysicsVolumes"))
	bool bDeformAllPhysicsBodies = false;

	/** Array with bones with physics bodies that will be deformed */
	UPROPERTY(EditAnywhere, Category = ReshapePhysics, meta = (EditCondition = "bReshapePhysicsVolumes && !bDeformAllPhysicsBodies"))
	TArray<FMeshReshapeBoneReference> PhysicsBodiesToDeform;
};

