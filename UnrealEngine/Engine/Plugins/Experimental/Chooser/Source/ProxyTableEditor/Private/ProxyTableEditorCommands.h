// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ProxyTableEditorCommands"
	
class FProxyTableEditorCommands : public TCommands<FProxyTableEditorCommands>
{
public:
	/** Constructor */
	FProxyTableEditorCommands() 
		: TCommands<FProxyTableEditorCommands>("ProxyTableEditor", NSLOCTEXT("Contexts", "ProxyTableEditor", "Proxy Table Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> EditTableSettings;

	/** Initialize commands */
	virtual void RegisterCommands() override
	{
		UI_COMMAND(EditTableSettings, "Table Settings", "Edit the root properties of the ChooserTable asset.", EUserInterfaceActionType::Button, FInputChord())
	}
};
	
#undef LOCTEXT_NAMESPACE
