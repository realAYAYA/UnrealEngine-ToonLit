// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Transform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshClipWithMesh.generated.h"

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

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	
	virtual UEdGraphPin* OutputPin() const override
	{
		return FindPin(TEXT("Material"));
	}

	UEdGraphPin* ClipMeshPin() const
	{
		return FindPin(TEXT("Clip Mesh"));
	}

	void BeginPostDuplicate(bool bDuplicateForPIE) override;

	// UCustomizableObjectNode interface
	void UpdateReferencedNodeId(const FGuid& NewGuid) override;

	// Details view variables
	// The clipping node uses tags
	bool bUseTags;

	// The clipping node uses a material name
	bool bUseMaterials;
};

