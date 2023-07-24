// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeReroute.h"

#include "EdGraph/EdGraphPin.h"

FText UPCGEditorGraphNodeReroute::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PCGEditorGraphNodeReroute", "NodeTitle", "Reroute");
}

bool UPCGEditorGraphNodeReroute::ShouldOverridePinNames() const
{
	return true;
}

FText UPCGEditorGraphNodeReroute::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	return FText::GetEmpty();
}

bool UPCGEditorGraphNodeReroute::CanSplitPin(const UEdGraphPin* Pin) const
{
	return false;
}

bool UPCGEditorGraphNodeReroute::ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const
{
	OutInputPinIndex = 0;
	OutOutputPinIndex = 1;
	return true;
}

FText UPCGEditorGraphNodeReroute::GetTooltipText() const
{
	return FText::GetEmpty();
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if (FromPin == GetInputPin())
	{
		return GetOutputPin();
	}
	else
	{
		return GetInputPin();
	}
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}

	return nullptr;
}
