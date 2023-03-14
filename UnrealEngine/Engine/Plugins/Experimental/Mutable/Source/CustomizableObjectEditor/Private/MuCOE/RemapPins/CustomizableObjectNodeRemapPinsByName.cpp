// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"


bool UCustomizableObjectNodeRemapPinsByName::Equal(const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const
{
	return Helper_GetPinName(&OldPin) == Helper_GetPinName(&NewPin) &&
				OldPin.PinType == NewPin.PinType &&
				OldPin.Direction == NewPin.Direction;
}


void UCustomizableObjectNodeRemapPinsByName::RemapPins(const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan)
{
	for (UEdGraphPin* OldPin : OldPins)
	{
		bool bFound = false;

		for (UEdGraphPin* NewPin : NewPins)
		{
			if (Equal(*OldPin, *NewPin))
			{
				bFound = true;

				PinsToRemap.Add(OldPin, NewPin);
				break;
			}
		}

		if (!bFound && OldPin->LinkedTo.Num())
		{
			PinsToOrphan.Add(OldPin);
		}
	}
}
