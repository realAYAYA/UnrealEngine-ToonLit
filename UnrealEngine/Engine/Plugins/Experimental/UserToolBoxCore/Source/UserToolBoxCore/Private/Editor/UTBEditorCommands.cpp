// Copyright Epic Games, Inc. All Rights Reserved.


#include "Editor/UTBEditorCommands.h"
#include "Styling/AppStyle.h"
#define LOCTEXT_NAMESPACE "UTBEditorCommands"
FUTBEditorCommands::FUTBEditorCommands()
	: TCommands<FUTBEditorCommands>
(
	TEXT("UsertoolboxEditor"),
	LOCTEXT("RemoteControl", "Remote Control API"),
	NAME_None,
	FAppStyle::GetAppStyleSetName()
	
)
{
}

FUTBEditorCommands::~FUTBEditorCommands()
{
}

void FUTBEditorCommands::RegisterCommands()
{
	// Rename Entity
	UI_COMMAND(RenameSection, "Rename", "Rename the selected  section.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));


}
#undef LOCTEXT_NAMESPACE