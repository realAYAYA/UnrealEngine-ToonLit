// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Recorder/TakeRecorderParameters.h"
#include "TakeRecorderSettings.generated.h"

class UTakePreset;

/**
 * Universal take recorder settings that apply to a whole take
 */
UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class UTakeRecorderUserSettings : public UObject
{
public:
	GENERATED_BODY()

	TAKERECORDER_API UTakeRecorderUserSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Return the PresetSaveDir path relative to the project. */
	FString GetResolvedPresetSaveDir() const;

	/** Sets the default location for the present saved directory. */
	void SetPresetSaveDir(const FString& InPath);

	/** User settings that should be passed to a recorder instance */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category="User Settings", meta=(ShowOnlyInnerProperties))
	FTakeRecorderUserParameters Settings;

	/** The default location in which to save take presets */
	UPROPERTY(config, EditAnywhere, Category="User Settings", DisplayName="Preset Save Location", meta = (ContentDir))
	FDirectoryPath PresetSaveDir;

	/** Soft reference to the preset last opened on the take recording UI */
	UPROPERTY(config)
	TSoftObjectPtr<UTakePreset> LastOpenedPreset;

	/** Whether the sequence editor is open for the take recorder */
	UPROPERTY(config)
	bool bIsSequenceOpen;

	/** Whether the sequence editor is open for the take recorder */
	UPROPERTY(config)
	bool bShowUserSettingsOnUI;
};

/**
 * Universal take recorder settings that apply to a whole take
 */
UCLASS(config=EditorSettings, MinimalAPI)
class UTakeRecorderProjectSettings : public UObject
{
public:
	GENERATED_BODY()

	TAKERECORDER_API UTakeRecorderProjectSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category="Take Recorder", meta=(ShowOnlyInnerProperties))
	FTakeRecorderProjectParameters Settings;

	/** Array of externally supplied CDOs that should be displayed on the take recorder project settings */
	TArray<TWeakObjectPtr<UObject>> AdditionalSettings;
};
