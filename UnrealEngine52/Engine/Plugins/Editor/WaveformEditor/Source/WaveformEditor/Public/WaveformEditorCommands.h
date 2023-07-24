// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Framework/Commands/Commands.h"

class FWaveformEditorCommands : public TCommands<FWaveformEditorCommands>
{
public:
	FWaveformEditorCommands();

	virtual void RegisterCommands() override;
	static const FWaveformEditorCommands& Get();

	// Soundwave Playback controls
	TSharedPtr<FUICommandInfo> PlaySoundWave;
	TSharedPtr<FUICommandInfo> PauseSoundWave;
	TSharedPtr<FUICommandInfo> StopSoundWave;
	TSharedPtr<FUICommandInfo> TogglePlayback;

	TSharedPtr<FUICommandInfo> ZoomIn;
	TSharedPtr<FUICommandInfo> ZoomOut;

	TSharedPtr<FUICommandInfo> ExportWaveform;
	TSharedPtr<FUICommandInfo> ExportFormatMono;
	TSharedPtr<FUICommandInfo> ExportFormatStereo;
};
