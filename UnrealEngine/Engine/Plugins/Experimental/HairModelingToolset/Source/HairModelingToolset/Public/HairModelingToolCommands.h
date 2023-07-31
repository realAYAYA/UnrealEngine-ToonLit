// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * TInteractiveToolCommands implementation for this module that provides standard Editor hotkey support
 */
class HAIRMODELINGTOOLSET_API FHairModelingToolCommands : public TCommands<FHairModelingToolCommands>
{
public:
	FHairModelingToolCommands();

	TSharedPtr<FUICommandInfo> BeginGroomToMeshTool;
	TSharedPtr<FUICommandInfo> BeginGenerateLODMeshesTool;
	TSharedPtr<FUICommandInfo> BeginGroomCardsEditorTool;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
