// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorArithmeticOp.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeColorArithmeticOp::UCustomizableObjectNodeColorArithmeticOp()
	: Super()
{
	Operation = EColorArithmeticOperation::E_Add;
}


void UCustomizableObjectNodeColorArithmeticOp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	//if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs") )
	{
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeColorArithmeticOp::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* XPin = CustomCreatePin(EGPD_Input, Schema->PC_Color, FName("A"));
	XPin->bDefaultValueIsIgnored = true;
	XPin->PinFriendlyName = FText();

	FName PinName = TEXT("B");
	UEdGraphPin* YPin = CustomCreatePin(EGPD_Input, Schema->PC_Color, PinName);
	YPin->bDefaultValueIsIgnored = true;
	YPin->PinFriendlyName = FText();

	PinName = TEXT("Result");
	UEdGraphPin* ColorPin = CustomCreatePin(EGPD_Output, Schema->PC_Color,PinName);
	ColorPin->bDefaultValueIsIgnored = true;
	ColorPin->PinFriendlyName = FText();
}


FText UCustomizableObjectNodeColorArithmeticOp::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView)
	{
		return LOCTEXT("Color_Arithmetic_Operation", "Color Arithmetic Operation");
	}

	const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/CustomizableObjectEditor.EColorArithmeticOperation"), true);

	if (!EnumPtr)
	{
		return FText::FromString(FString("Color Operation"));
	}

	const int32 index = EnumPtr->GetIndexByValue((int32)Operation);
	return EnumPtr->GetDisplayNameTextByIndex(index);
}


FLinearColor UCustomizableObjectNodeColorArithmeticOp::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeColorArithmeticOp::GetTooltipText() const
{
	return LOCTEXT("Color_Arithmetic_Tooltip", "Perform an arithmetic operation between two colors on a per-component basis.");
}


#undef LOCTEXT_NAMESPACE

