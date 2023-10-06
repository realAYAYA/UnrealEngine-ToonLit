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

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
