// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ChooserTableEditorCommands"
	
class FChooserTableEditorCommands : public TCommands<FChooserTableEditorCommands>
{
public:
	/** Constructor */
	FChooserTableEditorCommands() 
		: TCommands<FChooserTableEditorCommands>("ChooserTableEditor", NSLOCTEXT("Contexts", "ChooserTableEditor", "Chooser Table Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> EditChooserSettings;

	/** Initialize commands */
	virtual void RegisterCommands() override
	{
		UI_COMMAND(EditChooserSettings, "Table Settings", "Edit the root properties of the ChooserTable asset.", EUserInterfaceActionType::Button, FInputChord())
	}
};
	
#undef LOCTEXT_NAMESPACE
