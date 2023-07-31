// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorCommands.h"

#define LOCTEXT_NAMESPACE "MetasoundEditorCommands"

namespace Metasound
{
	namespace Editor
	{
		void FEditorCommands::RegisterCommands()
		{
			UI_COMMAND(Play, "Play", "Plays (or restarts) the MetaSound", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(Stop, "Stop", "Stops MetaSound (If currently playing)", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(TogglePlayback, "Toggle Playback", "Plays or stops the currently playing MetaSound", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));

			UI_COMMAND(Import, "Import", "Imports MetaSound from Json", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(Export, "Export", "Exports MetaSound to Json", EUserInterfaceActionType::Button, FInputChord());

			UI_COMMAND(BrowserSync, "Browse", "Selects the MetaSound in the content browser. If referencing MetaSound nodes are selected, selects referenced assets instead.", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(AddInput, "Add Input", "Adds an input to the node", EUserInterfaceActionType::Button, FInputChord());

			UI_COMMAND(EditMetasoundSettings, "MetaSound", "Edit MetaSound settings", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(EditSourceSettings, "Source", "Edit source specific settings", EUserInterfaceActionType::Button, FInputChord());

			UI_COMMAND(UpdateNodeClass, "Update Node Class", "Update selected node(s) class(es) that have an available update.", EUserInterfaceActionType::Button, FInputChord());

			UI_COMMAND(ConvertFromPreset, "Convert From Preset", "Converts this preset to a fully accessible MetaSound.", EUserInterfaceActionType::Button, FInputChord());
			UI_COMMAND(Delete, "Delete Selected", "Delete selected items.", EUserInterfaceActionType::None, FInputChord(EKeys::Delete));
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
