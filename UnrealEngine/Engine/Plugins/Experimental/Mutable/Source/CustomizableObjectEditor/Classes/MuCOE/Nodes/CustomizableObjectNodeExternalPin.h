// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Delegates/IDelegateInstance.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeExternalPin.generated.h"

class FArchive;
class UCustomizableObject;
class UCustomizableObjectNodeExposePin;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


/**
 * Custom remap pins by name action.
 *
 * If the node can not find the exposed pin, set the external pin as deprecated.
 */
UCLASS()
class UCustomizableObjectNodeRemapPinsCustomExternalPin : public UCustomizableObjectNodeRemapPinsByName
{
	GENERATED_BODY()

public:
	class UCustomizableObjectNodeExternalPin* Node = nullptr;

	virtual void RemapPins(const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;
};

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
	virtual void BeginConstruct() override;
	virtual void BackwardsCompatibleFixup() override;
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	UCustomizableObjectNodeRemapPins* CreateRemapPinsDefault() const override;
	void BeginPostDuplicate(bool bDuplicateForPIE) override;
	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;

	void UpdateReferencedNodeId(const FGuid& NewGuid) override;

	// Own interface
	
	/** Create the external pin. 
	 *
	 * @param NodeExposePin Used to obtain expose pin friendly name. If it is nullptr, the name will be set to a default.
	 */
	UEdGraphPin* CreateExternalPin(const UCustomizableObjectNodeExposePin* NodeExposePin);

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

