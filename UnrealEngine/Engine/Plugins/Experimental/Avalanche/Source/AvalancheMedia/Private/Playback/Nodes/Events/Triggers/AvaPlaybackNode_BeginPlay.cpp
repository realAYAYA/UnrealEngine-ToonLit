// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackNode_BeginPlay.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackNode_BeginPlay"

FText UAvaPlaybackNode_BeginPlay::GetNodeDisplayNameText() const
{
	return LOCTEXT("BeginPlayNode_Title", "Event Begin Play");
}

FText UAvaPlaybackNode_BeginPlay::GetNodeTooltipText() const
{
	return LOCTEXT("BeginPlayNode_Tooltip", "Triggers when the Playback Asset Starts Play");
}

void UAvaPlaybackNode_BeginPlay::NotifyPlaybackStateChanged(bool bPlaying)
{
	if (bPlaying)
	{
		TriggerEvent();
	}
}

#undef LOCTEXT_NAMESPACE