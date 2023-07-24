// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigStackCommands : public TCommands<FControlRigStackCommands>
{
public:
	FControlRigStackCommands() : TCommands<FControlRigStackCommands>
	(
		"ControlRigStack",
		NSLOCTEXT("Contexts", "RigStack", "Execution Stack"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Focuses on a selected operator's node */
	TSharedPtr< FUICommandInfo > FocusOnSelection;

	/** Looks for a specific instruction by index and brings it into focus */
	TSharedPtr< FUICommandInfo > GoToInstruction;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
