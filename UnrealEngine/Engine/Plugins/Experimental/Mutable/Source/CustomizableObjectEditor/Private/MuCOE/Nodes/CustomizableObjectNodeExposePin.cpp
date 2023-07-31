// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeExposePin.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExternalPin.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeExposePin::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("Name"))
	{
		OnNameChangedDelegate.Broadcast();
	}
}


void UCustomizableObjectNodeExposePin::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const bool bIsArrayPinCategory = PinType == UEdGraphSchema_CustomizableObject::PC_GroupProjector;
	UEdGraphPin* InputPin = CustomCreatePin(EGPD_Input, PinType, FName(TEXT("Object")), bIsArrayPinCategory);
}


FText UCustomizableObjectNodeExposePin::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FText PinCategoryName = UEdGraphSchema_CustomizableObject::GetPinCategoryName(PinType);

	if (TitleType == ENodeTitleType::ListView)
	{
		return FText::Format(LOCTEXT("Expose_Pin_Title_ListView", "Export {0} Pin"), PinCategoryName);
	}
	else
	{
		return FText::Format(LOCTEXT("Expose_Pin_Title", "{0}\nExport {1} Pin"), FText::FromString(Name), PinCategoryName);
	}
}


FLinearColor UCustomizableObjectNodeExposePin::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(PinType);
}


FText UCustomizableObjectNodeExposePin::GetTooltipText() const
{
	return LOCTEXT("Expose_Pin_Tooltip", "Exposes a value to the rest of its Customizable Object hierarchy.");
}

bool UCustomizableObjectNodeExposePin::CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	// Chech the pin types do match
	bOutArePinsCompatible = Super::CanConnect(InOwnedInputPin, InOutputPin, bOutIsOtherNodeBlocklisted, bOutArePinsCompatible);

	// Check the type of the other node to make sure it is not one we do not want to allow the connection with
	bOutIsOtherNodeBlocklisted = Cast<UCustomizableObjectNodeExternalPin>(InOutputPin->GetOwningNode()) != nullptr;

	return bOutArePinsCompatible && !bOutIsOtherNodeBlocklisted;
}


FString UCustomizableObjectNodeExposePin::GetNodeName() const
{
	return Name;
}


#undef LOCTEXT_NAMESPACE
