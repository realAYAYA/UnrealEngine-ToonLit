// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FTakeRecorderSourcesCommands : public TCommands<FTakeRecorderSourcesCommands>
{
public:
	FTakeRecorderSourcesCommands();

	TSharedPtr<FUICommandInfo> RecordSelectedActors;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
