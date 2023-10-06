// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "UObject/ObjectPtr.h"

#include "PlacementModeSubsystem.generated.h"

class UAssetPlacementSettings;

UCLASS()
class UPlacementModeSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem Interface

	// @returns the settings object for the mode for sharing across all tools and tool builders.
	const UAssetPlacementSettings* GetModeSettingsObject() const;
	UAssetPlacementSettings* GetMutableModeSettingsObject();

protected:
	UPROPERTY()
	TObjectPtr<UAssetPlacementSettings> ModeSettings;

	void SaveSettings() const;
	void LoadSettings();
};
