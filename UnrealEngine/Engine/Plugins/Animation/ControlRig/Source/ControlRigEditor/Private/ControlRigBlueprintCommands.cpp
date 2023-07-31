// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigBlueprintCommands"

void FControlRigBlueprintCommands::RegisterCommands()
{
	UI_COMMAND(DeleteItem, "Delete", "Deletes the selected items and removes their nodes from the graph.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(ExecuteGraph, "Execute", "Execute the rig graph if On.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(AutoCompileGraph, "Auto Compile", "Auto-compile the rig graph if On.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleEventQueue, "Toggle Event", "Toggle between the current and last running event", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConstructionEvent, "Construction Event", "Enable the construction mode for the rig", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ForwardsSolveEvent, "Forwards Solve", "Run the forwards solve graph", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BackwardsSolveEvent, "Backwards Solve", "Run the backwards solve graph", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BackwardsAndForwardsSolveEvent, "Backwards and Forwards", "Run backwards solve followed by forwards solve", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleExecutionMode, "Toggle Execution Mode", "Toggle between Release and Debug execution mode", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReleaseMode, "Release Mode", "Compiles and Executes the rig, ignoring debug data.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DebugMode, "Debug Mode", "Compiles and Executes the unoptimized rig, stopping at breakpoints.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResumeExecution, "Resume", "Resumes execution after being halted at a breakpoint.", EUserInterfaceActionType::Button, FInputChord(EKeys::F5));
	UI_COMMAND(ShowCurrentStatement, "Show Current Statement", "Focuses on the node currently being debugged.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StepInto, "Step Into", "Steps into the collapsed/function node, when halted at a breakpoint.", EUserInterfaceActionType::Button, FInputChord(EKeys::F11));
	UI_COMMAND(StepOut, "Step Out", "Steps out of the collapsed/function node, when halted at a breakpoint.", EUserInterfaceActionType::Button, FInputChord(EKeys::F11, EModifierKey::Shift));
	UI_COMMAND(StepOver, "Step Over", "Steps over the node, when halted at a breakpoint.", EUserInterfaceActionType::Button, FInputChord(EKeys::F10));
	UI_COMMAND(StoreNodeSnippet1, "Store Node Snippet 1", "Stores the selected node(s) into snippet 1", EUserInterfaceActionType::Button, FInputChord(EKeys::One, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet2, "Store Node Snippet 2", "Stores the selected node(s) into snippet 2", EUserInterfaceActionType::Button, FInputChord(EKeys::Two, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet3, "Store Node Snippet 3", "Stores the selected node(s) into snippet 3", EUserInterfaceActionType::Button, FInputChord(EKeys::Three, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet4, "Store Node Snippet 4", "Stores the selected node(s) into snippet 4", EUserInterfaceActionType::Button, FInputChord(EKeys::Four, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet5, "Store Node Snippet 5", "Stores the selected node(s) into snippet 5", EUserInterfaceActionType::Button, FInputChord(EKeys::Five, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet6, "Store Node Snippet 6", "Stores the selected node(s) into snippet 6", EUserInterfaceActionType::Button, FInputChord(EKeys::Six, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet7, "Store Node Snippet 7", "Stores the selected node(s) into snippet 7", EUserInterfaceActionType::Button, FInputChord(EKeys::Seven, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet8, "Store Node Snippet 8", "Stores the selected node(s) into snippet 8", EUserInterfaceActionType::Button, FInputChord(EKeys::Eight, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet9, "Store Node Snippet 9", "Stores the selected node(s) into snippet 9", EUserInterfaceActionType::Button, FInputChord(EKeys::Nine, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(StoreNodeSnippet0, "Store Node Snippet 0", "Stores the selected node(s) into snippet 0", EUserInterfaceActionType::Button, FInputChord(EKeys::Zero, EModifierKey::Alt | EModifierKey::Shift));
	UI_COMMAND(FrameSelection, "Frame Selection", "Frames the selected nodes in the Graph View.", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
}

#undef LOCTEXT_NAMESPACE
