// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FAssetPlacementEdModeCommands : public TCommands<FAssetPlacementEdModeCommands>
{
public:
	FAssetPlacementEdModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	TSharedPtr<FUICommandInfo> Select;
	TSharedPtr<FUICommandInfo> SelectAll;
	TSharedPtr<FUICommandInfo> LassoSelect;
	TSharedPtr<FUICommandInfo> Place;
	TSharedPtr<FUICommandInfo> PlaceSingle;
	TSharedPtr<FUICommandInfo> Erase;

protected:
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
