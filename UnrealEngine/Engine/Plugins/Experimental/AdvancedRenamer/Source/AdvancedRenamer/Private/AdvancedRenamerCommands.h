// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

class FAdvancedRenamerCommands : public TCommands<FAdvancedRenamerCommands>
{
public:
	FAdvancedRenamerCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> RenameSelectedActors;
	TSharedPtr<FUICommandInfo> RenameSharedClassActors;
};
