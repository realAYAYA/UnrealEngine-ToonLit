// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeMeshReshape.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FMeshReshapeBoneReference;
enum class EBoneDeformSelectionMethod : uint8;


UENUM()
enum class EMeshReshapeVertexColorChannelUsage
{
	None = 0,
	RigidClusterId = 1,
	MaskWeight = 2
};

USTRUCT()
struct FMeshReshapeColorUsage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	EMeshReshapeVertexColorChannelUsage R = EMeshReshapeVertexColorChannelUsage::None;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	EMeshReshapeVertexColorChannelUsage G = EMeshReshapeVertexColorChannelUsage::None;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	EMeshReshapeVertexColorChannelUsage B = EMeshReshapeVertexColorChannelUsage::None;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	EMeshReshapeVertexColorChannelUsage A = EMeshReshapeVertexColorChannelUsage::None;
};

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

	/** Enable the deformation of the vertices of the base mesh. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReshapeVertices = true;	

	/** Enable laplacian smoothing to the result of the base mesh reshape. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject, meta = (EditCondition = "bReshapeVertices"))
	bool bApplyLaplacianSmoothing = false;

	/** Enable the deformation of the skeleton of the base mesh. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReshapeSkeleton = false;	

	/** Enable the deformation of physics volumes of the base mesh */
    UPROPERTY(EditAnywhere, Category = CustomizableObject)
    bool bReshapePhysicsVolumes = false;
	
	UPROPERTY()
	bool bEnableRigidParts_DEPRECATED = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FMeshReshapeColorUsage VertexColorUsage;

	/** Bone Reshape Selection Method */
	UPROPERTY(EditAnywhere, Category = ReshapeBones, meta = (EditCondition = "bReshapeSkeleton"))
	EBoneDeformSelectionMethod SelectionMethod;

	/** Array with selected bones that will be deformed */
	UPROPERTY(EditAnywhere, Category = ReshapeBones, meta = (EditCondition = "bReshapeSkeleton && (SelectionMethod == EBoneDeformSelectionMethod::ONLY_SELECTED || SelectionMethod == EBoneDeformSelectionMethod::ALL_BUT_SELECTED)"))
	TArray<FMeshReshapeBoneReference> BonesToDeform;

	UPROPERTY(EditAnywhere, Category = ReshapePhysics, meta = (DisplayName = "Selection Method", EditCondition = "bReshapePhysicsVolumes"))
	EBoneDeformSelectionMethod PhysicsSelectionMethod;

	/** Array with bones with physics bodies that will be deformed */
	UPROPERTY(EditAnywhere, Category = ReshapePhysics, meta = (EditCondition = "bReshapePhysicsVolumes && (PhysicsSelectionMethod == EBoneDeformSelectionMethod::ONLY_SELECTED || PhysicsSelectionMethod == EBoneDeformSelectionMethod::ALL_BUT_SELECTED)"))
	TArray<FMeshReshapeBoneReference> PhysicsBodiesToDeform;

	UPROPERTY()
	bool bDeformAllBones_DEPRECATED = false;

	UPROPERTY()
	bool bDeformAllPhysicsBodies_DEPRECATED = false;
};

