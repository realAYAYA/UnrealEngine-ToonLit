// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementEdModeCommands.h"
#include "AssetPlacementEdModeStyle.h"
#include "AssetPlacementEdModeModule.h"

#define LOCTEXT_NAMESPACE "AssetPlacementEdMode"

FAssetPlacementEdModeCommands::FAssetPlacementEdModeCommands()
	: TCommands<FAssetPlacementEdModeCommands>("AssetPlacementEdMode",
		LOCTEXT("AssetPlacementEdModeCommands", "AssetPlacement Editor Mode Commands"),
		NAME_None,
		FAssetPlacementEdModeStyle::Get().GetStyleSetName())
{
}

void FAssetPlacementEdModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);
	UI_COMMAND(Select, "Select", "Select by clicking single assets matching the active palette.", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(Select);

#if !UE_IS_COOKED_EDITOR
	if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
	{
		UI_COMMAND(LassoSelect, "Lasso", "Selects asset by painting the area to select.", EUserInterfaceActionType::ToggleButton, FInputChord());
		ToolCommands.Add(LassoSelect);
		UI_COMMAND(Place, "Paint", "Paint mutliple assets from the active palette.", EUserInterfaceActionType::ToggleButton, FInputChord());
		ToolCommands.Add(Place);
	}
#endif // !UE_IS_COOKED_EDITOR

	UI_COMMAND(PlaceSingle, "Single", "Place a single, random asset from the active palette.", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(PlaceSingle);

#if !UE_IS_COOKED_EDITOR
	if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
	{
		UI_COMMAND(Erase, "Erase", "Paint to erase assets matching the active palette.", EUserInterfaceActionType::ToggleButton, FInputChord());
		ToolCommands.Add(Erase);
	}
#endif // !UE_IS_COOKED_EDITOR
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FAssetPlacementEdModeCommands::GetCommands()
{
	return FAssetPlacementEdModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
