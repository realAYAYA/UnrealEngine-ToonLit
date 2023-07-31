// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeExposePin.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


DECLARE_MULTICAST_DELEGATE(FOnNameChangedDelegate);

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExposePin : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// UObject interface
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	/** Return the Expose Pin Node expose pin name. */
	FString GetNodeName() const;
	
	// This is actually PinCategory
	UPROPERTY()
	FName PinType;
	
	UEdGraphPin* InputPin() const
	{
		return FindPin(TEXT("Object"));
	}

	/** Boradcasted when the UPROPERTY Name changes. */
	FOnNameChangedDelegate OnNameChangedDelegate;
	
private:
	UPROPERTY(Category = CustomizableObject, EditAnywhere)
	FString Name = "Default Name";
};

