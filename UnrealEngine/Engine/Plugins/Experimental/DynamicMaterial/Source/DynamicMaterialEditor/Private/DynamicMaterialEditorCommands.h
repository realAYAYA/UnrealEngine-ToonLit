// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FDynamicMaterialEditorCommands : public TCommands<FDynamicMaterialEditorCommands>
{
public:
	FDynamicMaterialEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> OpenEditorSettingsWindow;
	TSharedPtr<FUICommandInfo> AddDefaultLayer;
	TSharedPtr<FUICommandInfo> InsertDefaultLayerAbove;
	TSharedPtr<FUICommandInfo> SelectLayerBaseStage;
	TSharedPtr<FUICommandInfo> SelectLayerMaskStage;
	TSharedPtr<FUICommandInfo> MoveLayerUp;
	TSharedPtr<FUICommandInfo> MoveLayerDown;
};
