// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FAvaLevelEditorCommands : public TCommands<FAvaLevelEditorCommands>
{
public:
	FAvaLevelEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> CreateScene;

	TSharedPtr<FUICommandInfo> ActivateScene;

	TSharedPtr<FUICommandInfo> DeactivateScene;
};
