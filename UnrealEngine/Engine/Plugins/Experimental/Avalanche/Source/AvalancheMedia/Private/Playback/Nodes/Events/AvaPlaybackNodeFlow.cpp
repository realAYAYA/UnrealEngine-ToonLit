// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/Events/AvaPlaybackNodeFlow.h"
#include "Internationalization/Text.h"
#include "Playback/Nodes/AvaPlaybackNode.h"

FText UAvaPlaybackNodeFlow::GetNodeCategoryText() const
{
	return NodeCategory::EventFlowControl;
}
