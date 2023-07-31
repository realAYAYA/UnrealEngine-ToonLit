// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"


class FUICommandInfo;


class FMediaProfileCommands : public TCommands<FMediaProfileCommands>
{
public:

	FMediaProfileCommands()
		: TCommands<FMediaProfileCommands>(TEXT("ToolbarIcon"), NSLOCTEXT("Contexts", "ToolbarIcon", "Media Profile"), NAME_None, FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName())
	{}
	
	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

	/** Applies changes to the original Media Profile. */
	TSharedPtr<FUICommandInfo> Apply;

	/** Edit the current Media Profile. */
	TSharedPtr<FUICommandInfo> Edit;
};
