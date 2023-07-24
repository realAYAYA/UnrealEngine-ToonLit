// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Holds the UI commands for the MediaPlayerEditorToolkit widget.
 */
class FMediaPlayerEditorCommands
	: public TCommands<FMediaPlayerEditorCommands>
{
public:

	/** Default constructor. */
	FMediaPlayerEditorCommands() 
		: TCommands<FMediaPlayerEditorCommands>("MediaPlayerEditor", NSLOCTEXT("Contexts", "MediaPlayerEditor", "MediaPlayer Editor"), NAME_None, "MediaPlayerEditorStyle")
	{ }

public:

	//~ TCommands interface

	virtual void RegisterCommands() override;
	
public:

	/** Close the currently opened media. */
	TSharedPtr<FUICommandInfo> CloseMedia;

	/** Fast forwards media playback. */
	TSharedPtr<FUICommandInfo> ForwardMedia;

	/** Generate a thumnbnail. */
	TSharedPtr<FUICommandInfo> GenerateThumbnail;

	/** Jump to next item in the play list. */
	TSharedPtr<FUICommandInfo> NextMedia;

	/** Open the current media. */
	TSharedPtr<FUICommandInfo> OpenMedia;

	/** Pauses media playback. */
	TSharedPtr<FUICommandInfo> PauseMedia;

	/** Starts media playback. */
	TSharedPtr<FUICommandInfo> PlayMedia;

	/** Jump to previous item in the play list. */
	TSharedPtr<FUICommandInfo> PreviousMedia;

	/** Reverses media playback. */
	TSharedPtr<FUICommandInfo> ReverseMedia;

	/** Rewinds the media to the beginning. */
	TSharedPtr<FUICommandInfo> RewindMedia;
};
