// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConsoleVariablesEditorProjectSettings.generated.h"

UENUM(BlueprintType)
enum class EConsoleVariablesEditorRowDisplayType : uint8
{
	ShowCurrentValue,
	ShowLastEnteredValue
};

UCLASS(config = Engine, defaultconfig)
class CONSOLEVARIABLESEDITOR_API UConsoleVariablesEditorProjectSettings : public UObject
{
	GENERATED_BODY()
public:
	
	UConsoleVariablesEditorProjectSettings(const FObjectInitializer& ObjectInitializer)
	{
		UncheckedRowDisplayType = EConsoleVariablesEditorRowDisplayType::ShowCurrentValue;
		
		bAddAllChangedConsoleVariablesToCurrentPreset = true;

		ChangedConsoleVariableSkipList =
		{
			"Editor.ReflectEditorLevelVisibilityWithGame"
		};
	}

	/**
	 *When a row is unchecked, its associated variable's value will be set to the value recorded when the plugin was loaded.
	 *The value displayed to the user can be configured with this setting, but will not affect the actual applied value.
	 *ShowCurrentValue displays the actual value currently applied to the variable.
	 *ShowLastEnteredValue displays the value that will be applied when the row is checked.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Console Variables Editor")
	EConsoleVariablesEditorRowDisplayType UncheckedRowDisplayType;

	/**
	 *When variables are changed outside the Console Variables Editor, this option will add the variables to the current preset.
	 *Does not apply to console commands like 'r.SetNearClipPlane' or 'stat fps'
	 */
	UPROPERTY(Config, EditAnywhere, Category="Console Variables Editor")
	bool bAddAllChangedConsoleVariablesToCurrentPreset;

	/**
	 * If bAddAllChangedConsoleVariablesToCurrentPreset is true, this list will filter out any matching variables
	 * changed outside of the Console Variables Editor so they won't be added to the current preset.
	 * Matching variables explicitly added inside the Console Variables Editor will not be filtered.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Console Variables Editor")
	TSet<FString> ChangedConsoleVariableSkipList;
};
