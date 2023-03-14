// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Framework/Commands/Commands.h"

/**
 * Defines commands for the pixel mapping editor.
 */
class FDMXPixelMappingEditorCommands 
	: public TCommands<FDMXPixelMappingEditorCommands>
{
public:
	FDMXPixelMappingEditorCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> SaveThumbnailImage;
	TSharedPtr<FUICommandInfo> AddMapping;
	TSharedPtr<FUICommandInfo> PlayDMX;
	TSharedPtr<FUICommandInfo> StopPlayingDMX;
	TSharedPtr<FUICommandInfo> TogglePlayDMXAll;

	// Layout related
	TSharedPtr<FUICommandInfo> SizeComponentToTexture;

	TSharedPtr<FUICommandInfo> ToggleScaleChildrenWithParent;
	TSharedPtr<FUICommandInfo> ToggleAlwaysSelectGroup;
	TSharedPtr<FUICommandInfo> ToggleApplyLayoutScriptWhenLoaded;
	TSharedPtr<FUICommandInfo> ToggleShowComponentNames;
	TSharedPtr<FUICommandInfo> ToggleShowPatchInfo;
	TSharedPtr<FUICommandInfo> ToggleShowCellIDs;
};
