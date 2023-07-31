// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeRemapPinsByName.generated.h"

class UEdGraphPin;
class UObject;


/** 
 * Remap pins by name.
 * 
 * Remap pins by the field Pin->PinName. Output pins and input pins get remapped independently.
 * 
 * Use inheritance is a node requires a set of special rules when remapping by name.
 */
UCLASS()
class UCustomizableObjectNodeRemapPinsByName : public UCustomizableObjectNodeRemapPins
{
public:
	GENERATED_BODY()

	virtual bool Equal(const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const;
	
	virtual void RemapPins(const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;
};
