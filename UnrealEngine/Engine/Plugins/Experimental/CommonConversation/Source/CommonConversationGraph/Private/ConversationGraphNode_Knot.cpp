// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationGraphNode_Knot.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "EdGraph/EdGraphPin.h"
#include "SGraphNodeKnot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationGraphNode_Knot)

#define LOCTEXT_NAMESPACE "ConversationGraph"

static const char* PC_Wildcard = "wildcard";

/////////////////////////////////////////////////////
// UConversationGraphNode_Knot

UConversationGraphNode_Knot::UConversationGraphNode_Knot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}

void UConversationGraphNode_Knot::AllocateDefaultPins()
{
	const FName InputPinName(TEXT("InputPin"));
	const FName OutputPinName(TEXT("OutputPin"));

	UEdGraphPin* MyInputPin = CreatePin(EGPD_Input, PC_Wildcard, InputPinName);
	MyInputPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* MyOutputPin = CreatePin(EGPD_Output, PC_Wildcard, OutputPinName);
}

FText UConversationGraphNode_Knot::GetTooltipText() const
{
	//@TODO: Should pull the tooltip from the source pin
	return LOCTEXT("KnotTooltip", "Reroute Node (reroutes wires)");
}

FText UConversationGraphNode_Knot::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromString(NodeComment);
	}
	else if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("KnotListTitle", "Add Reroute Node...");
	}
	else
	{
		return LOCTEXT("KnotTitle", "Reroute Node");
	}
}

bool UConversationGraphNode_Knot::ShouldOverridePinNames() const
{
	return true;
}

FText UConversationGraphNode_Knot::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	// Keep the pin size tiny
	return FText::GetEmpty();
}

void UConversationGraphNode_Knot::OnRenameNode(const FString& NewName)
{
	NodeComment = NewName;
}

bool UConversationGraphNode_Knot::CanSplitPin(const UEdGraphPin* Pin) const
{
	return false;
}

TSharedPtr<class INameValidatorInterface> UConversationGraphNode_Knot::MakeNameValidator() const
{
	// Comments can be duplicated, etc...
	return MakeShareable(new FDummyNameValidator(EValidatorResult::Ok));
}

UEdGraphPin* UConversationGraphNode_Knot::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if(FromPin && Pins.Contains(FromPin))
	{
		return FromPin == Pins[0] ? Pins[1] : Pins[0];
	}

	return nullptr;
}

TSharedPtr<SGraphNode> UConversationGraphNode_Knot::CreateVisualWidget()
{
	return SNew(SGraphNodeKnot, this);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

