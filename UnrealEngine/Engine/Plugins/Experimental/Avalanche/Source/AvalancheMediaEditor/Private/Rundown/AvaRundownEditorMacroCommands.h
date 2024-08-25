// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownEditorMacroCommands.generated.h"

/** Available macro commands for Ava rundown editor. */
UENUM()
enum class EAvaRundownEditorMacroCommand : uint8
{
	None,
	// Playback commands
	LoadPage,
	UnloadPage,
	TakeIn,
	TakeOut,
	ForceTakeOut,
	TakeNext,
	Continue,
	PreviewIn,
	PreviewOut,
	ForcePreviewOut,
	PreviewNext,
	ContinuePreview,
	TakeToProgram,
	StartChannel,
	StopChannel
};

struct FAvaRundownEditorMacroCommands
{
	static FName GetCommandName(EAvaRundownEditorMacroCommand InCommand);
	static FName GetShortCommandName(EAvaRundownEditorMacroCommand InCommand);
};
