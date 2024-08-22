// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "CustomEditorStyle.h"

class FCustomEditorCommands : public TCommands<FCustomEditorCommands>
{
	
public:

	FCustomEditorCommands()
		: TCommands<FCustomEditorCommands>(TEXT("DEditor"), NSLOCTEXT("Contexts", "DEditor", "DEditor Plugin"), NAME_None, FCustomEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	
	TSharedPtr<FUICommandInfo> StartGameService;
	TSharedPtr<FUICommandInfo> StopGameService;
	TSharedPtr<FUICommandInfo> ShowGddInFileExplorer;
	TSharedPtr<FUICommandInfo> ShowExcelInFileExplorer;
	TSharedPtr<FUICommandInfo> UpdateGdd;
	TSharedPtr<FUICommandInfo> ReloadGdd;
	TSharedPtr<FUICommandInfo> UpdatePb;

	/** Only Blueprints*/
	TSharedPtr<FUICommandInfo> GenerateWidgetTsFile;
};
