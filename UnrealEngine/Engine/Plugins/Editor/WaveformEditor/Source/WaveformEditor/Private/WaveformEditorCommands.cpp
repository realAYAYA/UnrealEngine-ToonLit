// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorCommands.h"

#define LOCTEXT_NAMESPACE "WaveformEditorCommands"

FWaveformEditorCommands::FWaveformEditorCommands() 
	: TCommands<FWaveformEditorCommands>(
		TEXT("WaveformEditor"), 
		NSLOCTEXT("Contexts", "WaveformEditor", "Waveform Editor"), 
		NAME_None, 
		"AudioStyleSet"
	)
{}

void FWaveformEditorCommands::RegisterCommands()
{
	UI_COMMAND(PlaySoundWave, "Play", "Play this sound wave", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PauseSoundWave, "Pause", "Pause this sound wave", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopSoundWave, "Stop", "Stops playback and rewinds to beginning of file", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(TogglePlayback, "TogglePlayback", "Toggles between play and pause", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));

	UI_COMMAND(ZoomIn, "Zoom In", "Zooms into the sound wave", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ZoomOut, "Zoom Out", "Zooms out from the sound wave", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ExportWaveform, "Export Waveform", "Exports the edited waveform to a new sound wave uasset", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportFormatMono, "Mono", "Sets the export format to mono", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ExportFormatStereo, "Stereo", "Sets the export format to stereo", EUserInterfaceActionType::RadioButton, FInputChord());
}
	
const FWaveformEditorCommands& FWaveformEditorCommands::Get()
{
	return TCommands<FWaveformEditorCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
