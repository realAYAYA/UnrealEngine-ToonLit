// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Editor/RigVMEditorStyle.h"

class FControlRigEditorCommands : public TCommands<FControlRigEditorCommands>
{
public:
	FControlRigEditorCommands() : TCommands<FControlRigEditorCommands>
	(
		"ControlRigBlueprint",
		NSLOCTEXT("Contexts", "Animation", "Rig Blueprint"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FRigVMEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Enable the construction mode for the rig */
	TSharedPtr< FUICommandInfo > ConstructionEvent;

	/** Run the forwards solve graph */
	TSharedPtr< FUICommandInfo > ForwardsSolveEvent;

	/** Run the backwards solve graph */
	TSharedPtr< FUICommandInfo > BackwardsSolveEvent;

	/** Run the backwards solve graph followed by the forwards solve graph */
	TSharedPtr< FUICommandInfo > BackwardsAndForwardsSolveEvent;

	/** Request per node direct manipulation on a position */
	TSharedPtr< FUICommandInfo > RequestDirectManipulationPosition;

	/** Request per node direct manipulation on a rotation */
	TSharedPtr< FUICommandInfo > RequestDirectManipulationRotation;

	/** Request per node direct manipulation on a scale */
	TSharedPtr< FUICommandInfo > RequestDirectManipulationScale;

	/** Toggle visibility of the schematic */
	TSharedPtr< FUICommandInfo > ToggleSchematicViewportVisibility;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
