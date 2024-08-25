// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FChaosVDCommands : public TCommands<FChaosVDCommands>
{
public:

	FChaosVDCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> TrackUntrackSelectedObject;
};