// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationGraphNode_EntryPoint.h"
#include "UObject/UObjectIterator.h"
#include "ConversationGraphTypes.h"
#include "ConversationGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationGraphNode_EntryPoint)

UConversationGraphNode_EntryPoint::UConversationGraphNode_EntryPoint(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UConversationGraphNode_EntryPoint::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UConversationGraphTypes::PinCategory_MultipleNodes, TEXT("Out"));
}

FName UConversationGraphNode_EntryPoint::GetNameIcon() const
{
	return FName("BTEditor.Graph.BTNode.Root.Icon");
}

