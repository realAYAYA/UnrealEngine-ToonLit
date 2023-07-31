// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"

#define LOCTEXT_NAMESPACE "FRewindDebuggerCommands"

class FRewindDebuggerCommands : public TCommands<FRewindDebuggerCommands>
{
public:

	/** Default constructor. */
	FRewindDebuggerCommands()
		: TCommands<FRewindDebuggerCommands>(TEXT("RewindDebugger"), LOCTEXT("RewindDebugger", "Rewind Debugger"), NAME_None, "RewindDebuggerStyle")
	{ }

	// TCommands interface

	virtual void RegisterCommands() override
	{
		UI_COMMAND(PauseOrPlay, "Pause or Play Recording", "Toggle recording playing", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar) );
		UI_COMMAND(StartRecording, "Start Recording", "Start recording Animation Insights data", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::R));
		UI_COMMAND(StopRecording, "Stop Recording", "Stop recording Animation Insights data", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
		UI_COMMAND(FirstFrame, "First Frame", "Jump to first recorded frame", EUserInterfaceActionType::Button, FInputChord(EKeys::Up));
		UI_COMMAND(PreviousFrame, "Previous Frame", "Step one frame back", EUserInterfaceActionType::Button, FInputChord(EKeys::Left));
		UI_COMMAND(ReversePlay, "Reverse Play", "Playback recorded data in reverse", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::SpaceBar));
		UI_COMMAND(Pause, "Pause", "Pause playback", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Play, "Play", "Playback recorded data", EUserInterfaceActionType::Button, FInputChord(EKeys::Down));
		UI_COMMAND(NextFrame, "Next Frame", "Step one frame forward", EUserInterfaceActionType::Button, FInputChord(EKeys::Right));
		UI_COMMAND(LastFrame, "Last Frame", "Jump to last recorded frame", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Up));
	}


	TSharedPtr<FUICommandInfo> StartRecording;
	TSharedPtr<FUICommandInfo> StopRecording;
	TSharedPtr<FUICommandInfo> FirstFrame;
	TSharedPtr<FUICommandInfo> PreviousFrame;
	TSharedPtr<FUICommandInfo> ReversePlay;
	TSharedPtr<FUICommandInfo> Pause;
	TSharedPtr<FUICommandInfo> Play;
	TSharedPtr<FUICommandInfo> NextFrame;
	TSharedPtr<FUICommandInfo> LastFrame;
	TSharedPtr<FUICommandInfo> PauseOrPlay;
};


#undef LOCTEXT_NAMESPACE
