// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FExampleAssetEditorCommands : public TCommands<FExampleAssetEditorCommands>
{
public:

	FExampleAssetEditorCommands()
		: TCommands<FExampleAssetEditorCommands>(TEXT("ExampleAssetEditor"), NSLOCTEXT("Contexts", "FExampleAssetEditorModule", "Example Asset Editor Plugin"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};