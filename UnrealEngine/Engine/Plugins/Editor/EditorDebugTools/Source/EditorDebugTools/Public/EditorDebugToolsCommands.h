// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorDebugToolsStyle.h"

class FEditorDebugToolsCommands : public TCommands<FEditorDebugToolsCommands>
{
public:

	FEditorDebugToolsCommands()
		: TCommands<FEditorDebugToolsCommands>(TEXT("EditorDebugTools"), NSLOCTEXT("Contexts", "EditorDebugTools", "EditorDebugTools Plugin"), NAME_None, FEditorDebugToolsStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};