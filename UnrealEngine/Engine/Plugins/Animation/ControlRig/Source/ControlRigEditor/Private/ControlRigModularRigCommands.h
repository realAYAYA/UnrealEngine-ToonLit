// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigModularRigCommands : public TCommands<FControlRigModularRigCommands>
{
public:
	FControlRigModularRigCommands() : TCommands<FControlRigModularRigCommands>
	(
		"ControlRigModularRigModel",
		NSLOCTEXT("Contexts", "ModularRigModel", "Modular Rig Modules"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Add Module at root */
	TSharedPtr< FUICommandInfo > AddModuleItem;

	/** Rename Module */
	TSharedPtr< FUICommandInfo > RenameModuleItem;

	/** Delete Module */
	TSharedPtr< FUICommandInfo > DeleteModuleItem;

	/** Mirror Module */
	TSharedPtr< FUICommandInfo > MirrorModuleItem;

	/** Reresolve Module */
	TSharedPtr< FUICommandInfo > ReresolveModuleItem;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
