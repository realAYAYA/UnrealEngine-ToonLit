// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownEditorMacroCommands.h"

#include "UObject/Class.h"

FName FAvaRundownEditorMacroCommands::GetCommandName(EAvaRundownEditorMacroCommand InCommand)
{
	return StaticEnum<EAvaRundownEditorMacroCommand>()->GetNameByValue(static_cast<int64>(InCommand));
}

FName FAvaRundownEditorMacroCommands::GetShortCommandName(EAvaRundownEditorMacroCommand InCommand)
{
	return FName(StaticEnum<EAvaRundownEditorMacroCommand>()->GetNameStringByValue(static_cast<int64>(InCommand)));
}