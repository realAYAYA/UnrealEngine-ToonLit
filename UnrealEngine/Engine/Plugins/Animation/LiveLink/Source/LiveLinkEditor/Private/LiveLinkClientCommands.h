// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FLiveLinkClientCommands : public TCommands<FLiveLinkClientCommands>
{
public:
	FLiveLinkClientCommands();

	TSharedPtr<FUICommandInfo> RemoveSource;
	TSharedPtr<FUICommandInfo> RemoveAllSources;
	TSharedPtr<FUICommandInfo> RemoveSubject;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
