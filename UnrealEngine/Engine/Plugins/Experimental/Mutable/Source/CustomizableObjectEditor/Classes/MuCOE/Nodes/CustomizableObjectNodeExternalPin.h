// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "CustomizableObjectNodeExternalPin.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObject;
class UCustomizableObjectNodeExposePin;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExternalPin : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	
	// EdGraphNode interface 
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	
	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	UCustomizableObjectNodeRemapPins* CreateRemapPinsDefault() const override;
	void BeginPostDuplicate(bool bDuplicateForPIE) override;
	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;

	void UpdateReferencedNodeId(const FGuid& NewGuid) override;
	
	/** Set the linked Node Expose Pin node guid. */
	void SetExternalObjectNodeId(FGuid Guid);

	/** Return the external pin. Can return nullptr. */
	UEdGraphPin* GetExternalPin() const;

	/** Return the linked Expose Pin node. Return nullptr if not set. */
	UCustomizableObjectNodeExposePin* GetNodeExposePin() const;

	// This is actually PinCategory
	UPROPERTY()
	FName PinType;

	/** External Customizable Object which the linked Node Expose Pin belong to. */
	UPROPERTY()
	TObjectPtr<UCustomizableObject> ExternalObject;

private:
	/** Linked Node Expose Pin node guid. */
	UPROPERTY()
	FGuid ExternalObjectNodeId;
	
	FDelegateHandle OnNameChangedDelegateHandle;
	FDelegateHandle DestroyNodeDelegateHandle;
};

