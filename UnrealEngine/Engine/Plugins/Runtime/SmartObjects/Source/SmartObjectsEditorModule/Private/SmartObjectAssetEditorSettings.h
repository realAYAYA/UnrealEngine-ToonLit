// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SmartObjectAssetEditorSettings.generated.h"

/**
 * Settings for the SmartObject asset editor
 */
UCLASS(config=EditorPerProjectUserSettings)
class USmartObjectAssetEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USmartObjectAssetEditorSettings();

	/** Indicates whether or not the grid must be shown by default when the editor is opened */
	UPROPERTY(config, EditAnywhere, Category="Grid")
	bool bShowGridByDefault;
};
