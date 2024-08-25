// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/TG_PinSelectionManager.h"
#include "EdGraph/TG_EdGraphNode.h"

void FTG_PinSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	//Collector.AddReferencedObjects(SelectedItems);
}

void FTG_PinSelectionManager::UpdateSelection(UEdGraphPin* Pin)
{
	OnPinSelectionUpdated.Broadcast(Pin);
}
