// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/SchemaActions/AvaPlaybackAction_PasteNode.h"

#include "Playback/AvaPlaybackGraph.h"
#include "Playback/Graph/AvaPlaybackEditorGraph.h"
#include "Playback/IAvaPlaybackGraphEditor.h"

UEdGraphNode* FAvaPlaybackAction_PasteNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin
	, const FVector2D Location, bool bSelectNewNode)
{
	UAvaPlaybackGraph* const Playback = CastChecked<UAvaPlaybackEditorGraph>(ParentGraph)->GetPlaybackGraph();
	check(Playback);
	
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = Playback->GetGraphEditor())
	{
		GraphEditor->PasteNodesHere(Location);
	}
	
	return nullptr;
}
