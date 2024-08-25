// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackTabFactory.h"

#include "Playback/AvaPlaybackGraphEditor.h"

FAvaPlaybackTabFactory::FAvaPlaybackTabFactory(const FName& InTabID, const TSharedPtr<FAvaPlaybackGraphEditor>& InPlaybackEditor)
	: FWorkflowTabFactory(InTabID, InPlaybackEditor)
	, PlaybackEditorWeak(InPlaybackEditor)
{
}
