// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
