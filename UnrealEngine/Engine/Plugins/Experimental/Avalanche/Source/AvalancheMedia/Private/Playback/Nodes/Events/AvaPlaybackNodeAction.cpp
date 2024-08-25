// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/Events/AvaPlaybackNodeAction.h"
#include "Internationalization/Text.h"
#include "Playback/Nodes/AvaPlaybackNode.h"

FText UAvaPlaybackNodeAction::GetNodeCategoryText() const
{
	return NodeCategory::EventAction;
}
