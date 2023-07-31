// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigBlueprintCommands : public TCommands<FControlRigBlueprintCommands>
{
public:
	FControlRigBlueprintCommands() : TCommands<FControlRigBlueprintCommands>
	(
		"ControlRigBlueprint",
		NSLOCTEXT("Contexts", "Animation", "Rig Blueprint"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Deletes the selected items and removes their nodes from the graph. */
	TSharedPtr< FUICommandInfo > DeleteItem;

	/** Toggle Execute the Graph */
	TSharedPtr< FUICommandInfo > ExecuteGraph;

	/** Toggle Auto Compilation in the Graph */
	TSharedPtr< FUICommandInfo > AutoCompileGraph;

	/** Toggle between this and the last event queue */
	TSharedPtr< FUICommandInfo > ToggleEventQueue;

	/** Enable the construction mode for the rig */
	TSharedPtr< FUICommandInfo > ConstructionEvent;

	/** Run the forwards solve graph */
	TSharedPtr< FUICommandInfo > ForwardsSolveEvent;

	/** Run the backwards solve graph */
	TSharedPtr< FUICommandInfo > BackwardsSolveEvent;

	/** Run the backwards solve graph followed by the forwards solve graph */
	TSharedPtr< FUICommandInfo > BackwardsAndForwardsSolveEvent;

	/** Toggle between Release and Debug execution mode */
	TSharedPtr< FUICommandInfo > ToggleExecutionMode;

	/** Compile and run the optimized rig, ignoring any debug data */
	TSharedPtr< FUICommandInfo > ReleaseMode;

	/** Compile and run the unoptimized rig, ignoring any debug data */
	TSharedPtr< FUICommandInfo > DebugMode;
	
	/** Resume the execution of the graph when halted at a breakpoint */
	TSharedPtr< FUICommandInfo > ResumeExecution;

	/** Focuses on the node currently being debugged */
	TSharedPtr< FUICommandInfo > ShowCurrentStatement;

	/** Steps to the next node in the execution graph (at the same graph level) when halted at a breakpoint */
	TSharedPtr< FUICommandInfo > StepOver;

	/** Steps into the collapsed/function node, when halted at a breakpoint */
	TSharedPtr< FUICommandInfo > StepInto;

	/** Steps out of the collapsed/function node, when halted at a breakpoint */
	TSharedPtr< FUICommandInfo > StepOut;

	/** Stores the selected node(s) into snippet 1. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet1;

	/** Stores the selected node(s) into snippet 2. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet2;

	/** Stores the selected node(s) into snippet 3. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet3;

	/** Stores the selected node(s) into snippet 4. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet4;

	/** Stores the selected node(s) into snippet 5. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet5;

	/** Stores the selected node(s) into snippet 6. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet6;

	/** Stores the selected node(s) into snippet 7. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet7;

	/** Stores the selected node(s) into snippet 8. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet8;

	/** Stores the selected node(s) into snippet 9. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet9;

	/** Stores the selected node(s) into snippet 0. */
	TSharedPtr< FUICommandInfo > StoreNodeSnippet0;

	/** Frames the selected nodes */
	TSharedPtr< FUICommandInfo > FrameSelection;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
