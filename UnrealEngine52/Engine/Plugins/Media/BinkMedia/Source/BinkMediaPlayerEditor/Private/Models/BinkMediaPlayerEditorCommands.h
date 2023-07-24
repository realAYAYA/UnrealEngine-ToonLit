// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#pragma once

#include "Framework/Commands/Commands.h"

struct FBinkMediaPlayerEditorCommands : public TCommands<FBinkMediaPlayerEditorCommands> 
{
	FBinkMediaPlayerEditorCommands() : TCommands<FBinkMediaPlayerEditorCommands>("BinkMediaPlayerEditor", NSLOCTEXT("Contexts", "BinkMediaPlayerEditor", "Bink MediaPlayer Editor"), NAME_None, "BinkMediaPlayerEditorStyle") 
	{ 
	}

#define LOCTEXT_NAMESPACE ""
	virtual void RegisterCommands() override 
	{
		UI_COMMAND(PauseMedia, "Pause", "Pause media playback", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(PlayMedia, "Play", "Start media playback", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(RewindMedia, "Rewind", "Rewinds the media to the beginning", EUserInterfaceActionType::Button, FInputChord());
	}
#undef LOCTEXT_NAMESPACE
	
	TSharedPtr<FUICommandInfo> ForwardMedia;
	TSharedPtr<FUICommandInfo> PauseMedia;
	TSharedPtr<FUICommandInfo> PlayMedia;
	TSharedPtr<FUICommandInfo> ReverseMedia;
	TSharedPtr<FUICommandInfo> RewindMedia;
};
