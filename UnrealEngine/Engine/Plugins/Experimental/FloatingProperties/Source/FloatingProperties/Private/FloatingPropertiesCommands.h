// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FFloatingPropertiesCommands : public TCommands<FFloatingPropertiesCommands>
{
public:
	FFloatingPropertiesCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> ToggleEnabled;
};
