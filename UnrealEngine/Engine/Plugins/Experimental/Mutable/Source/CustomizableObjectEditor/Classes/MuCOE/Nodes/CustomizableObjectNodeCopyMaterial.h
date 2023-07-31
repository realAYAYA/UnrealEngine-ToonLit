// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeCopyMaterial.generated.h"

class UCustomizableObjectNodeRemapPins;
class UCustomizableObjectNodeSkeletalMesh;
class UEdGraphPin;
class UObject;

/**
 * Copy Material node. Duplicates a Material Node. Duplicates all Material node input pins and properties except for the Mesh input pin.
 * A new Mesh has to be defined through the new Mesh input pin.
 *
 * Input pins:
 * - Mesh: New mesh.
 * - Material: Material to duplicate.
 * 
 * Output pins:
 * - Material: New material.
 * 
 * Properties:
 *   [NONE]
 */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeCopyMaterial : public UCustomizableObjectNodeMaterial
{
public:
	GENERATED_BODY()

	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	/** Get Mesh input pin. */
	UEdGraphPin* GetMeshPin() const override;

	/** Get Material input pin. */
	UEdGraphPin* GetMaterialPin() const;

	/** Get Mesh node. */
	UCustomizableObjectNodeSkeletalMesh* GetMeshNode() const;

	/** Get the Material node. */
	UCustomizableObjectNodeMaterial* GetMaterialNode() const;

	/** Input material pin can only be connect to a NodeMaterial output pin or a NodeExternalPin. A connection to a NodeMaterialCopy output pin is not allowed. */
	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;
	
	bool ShouldBreakExistingConnections(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const override;

	/** Override the NodeMaterial method. Does not break any connection. */
	void BreakExistingConnectionsPostConnection(UEdGraphPin* InputPin, UEdGraphPin* OutputPin) override {};

	virtual bool IsNodeOutDatedAndNeedsRefresh() override;

	bool ProvidesCustomPinRelevancyTest() const override;

	/** 
	 * Only are relevant pins:
	 * - From: NodeMaterial, excluding NodeCopyMaterial 
	 * - To: NodeBaseObject
	 */
	bool IsPinRelevant(const UEdGraphPin* Pin) const override;

	FText GetTooltipText() const override;
};