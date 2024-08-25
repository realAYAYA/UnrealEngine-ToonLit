// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/Events/AvaPlaybackNodeTrigger.h"
#include "AvaMediaDefines.h"
#include "Internationalization/Text.h"
#include "Playback/Nodes/AvaPlaybackNode.h"

FText UAvaPlaybackNodeTrigger::GetNodeCategoryText() const
{
	return NodeCategory::EventTrigger;
}

void UAvaPlaybackNodeTrigger::TriggerEvent()
{
	bEventTriggered = true;
}

void UAvaPlaybackNodeTrigger::Reset()
{
	bEventTriggered = false;
}

void UAvaPlaybackNodeTrigger::TickEvent(float DeltaTime, FAvaPlaybackEventParameters& OutEventParameters)
{
	if (bEventTriggered)
	{
		OutEventParameters.RequestTriggerEventAction();
	}
}
