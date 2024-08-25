// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeMeshClipWithMesh.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FGuid;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshClipWithMesh : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshClipWithMesh();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClip)
	TArray<FString> Tags;

	UPROPERTY(EditAnywhere, Category = CustomizableObjectToClip)
	EMutableMultipleTagPolicy MultipleTagPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

	//!< If assigned, then a material inside this CO will be clipped by this node.
    //!< If several materials with the same name, all are considered (to cover all LOD levels)
    UPROPERTY(EditAnywhere, Category = CustomizableObjectToClip)
	TObjectPtr<UCustomizableObject> CustomizableObjectToClipWith;

    //!< Array with the Guids of the nodes with the same material inside the CustomizableObjectToClipWith CO (if any is assigned)
    UPROPERTY(EditAnywhere, Category = CustomizableObjectToClip)
    TArray<FGuid> ArrayMaterialNodeToClipWithID;

	/** Mesh Transform*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FTransform Transform;

	// Details view variables
	// The clipping node uses tags
	bool bUseTags;

	// The clipping node uses a material name
	bool bUseMaterials;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void UpdateReferencedNodeId(const FGuid& NewGuid) override;
	virtual void BeginPostDuplicate(bool bDuplicateForPIE) override;

	// UCustomizableObjectNodeMaterialBase interface
	virtual UEdGraphPin* OutputPin() const override;

	// Own interface
	UEdGraphPin* ClipMeshPin() const;
};

