// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetSearchCommands.h"
#include "Styling/AppStyle.h" 

#define LOCTEXT_NAMESPACE "AssetSearchCommands"

FAssetSearchCommands::FAssetSearchCommands() : TCommands<FAssetSearchCommands>(
	"AssetSearchCommands",
	NSLOCTEXT("Contexts", "AssetSearchCommands", "Asset Search"),
	NAME_None, 
	FAppStyle::GetAppStyleSetName())
{
}

void FAssetSearchCommands::RegisterCommands()
{
	UI_COMMAND(ViewAssetSearch, "Search...", "Show Search Tab", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
