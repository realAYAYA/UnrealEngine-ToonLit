// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet2/WildcardNodeUtils.h"

#include "Containers/Array.h"
#include "EdGraphSchema_K2.h"
#include "Misc/AssertionMacros.h"
#include "UObject/UnrealNames.h"
#include "UObject/WeakObjectPtrTemplates.h"

FEdGraphPinType FWildcardNodeUtils::GetDefaultWildcardPinType()
{
	static FEdGraphPinType WildcardPinType(
		/* InPinCategory */ UEdGraphSchema_K2::PC_Wildcard,
		/* InPinSubCategory */ NAME_None,
		/* InPinSubCategoryObject */ nullptr,
		/* InPinContainerType */ EPinContainerType::None,
		/* bInIsReference */ false,
		/* InValueTerminalType */{}
	);

	return WildcardPinType;
}

bool FWildcardNodeUtils::IsWildcardPin(const UEdGraphPin* const Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard;
}

bool FWildcardNodeUtils::IsLinkedToWildcard(const UEdGraphPin* const Pin)
{
	if (Pin)
	{
		for (const UEdGraphPin* const LinkedPin : Pin->LinkedTo)
		{
			if (FWildcardNodeUtils::IsWildcardPin(LinkedPin))
			{
				return true;
			}
		}
	}
	return false;
}

UEdGraphPin* FWildcardNodeUtils::CreateWildcardPin(UEdGraphNode* Node, const FName PinName, const EEdGraphPinDirection Direction, const EPinContainerType ContainerType/* = EPinContainerType::None*/)
{
	check(Node);

	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinType.PinSubCategory = NAME_None;
	PinType.PinSubCategoryObject = nullptr;
	PinType.bIsReference = false;
	PinType.ContainerType = ContainerType;

	return Node->CreatePin(Direction, PinType, PinName);
}

bool FWildcardNodeUtils::NodeHasAnyWildcards(const UEdGraphNode* const Node)
{
	check(Node);

	for (const UEdGraphPin* const Pin : Node->Pins)
	{
		if (FWildcardNodeUtils::IsWildcardPin(Pin))
		{
			return true;
		}
	}

	return false;
}