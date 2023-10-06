// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "EnhancedInputEditorSettings.generated.h"

struct FDefaultContextSetting;

class UEnhancedPlayerInput;
class UEnhancedInputEditorSubsystem;

/** Settings for the Enhanced Input Editor Subsystem that are persistent between a project's users */
UCLASS(config = Input, defaultconfig, meta = (DisplayName = "Enhanced Input (Editor Only)"))
class UEnhancedInputEditorProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public: 
	UEnhancedInputEditorProjectSettings(const FObjectInitializer& Initializer);
	
	/** The default player input class that the Enhanced Input Editor subsystem will use. */
	UPROPERTY(config, EditAnywhere, NoClear, Category = Default)
	TSoftClassPtr<UEnhancedPlayerInput> DefaultEditorInputClass;

	/** Array of any input mapping contexts that you want to always be applied to the Enhanced Input Editor Subsystem. */
	UPROPERTY(config, EditAnywhere, Category = Default)
	TArray<FDefaultContextSetting> DefaultMappingContexts;
};

/**
 * A collection of useful individual user settings when using the EnhancedInputEditorSubsystem.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Enhanced Input Editor Settings"))
class INPUTEDITOR_API UEnhancedInputEditorSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:

	UEnhancedInputEditorSettings();

	/**
	 * If true then the Enhanced Input Editor subsystem will log all input that is being processed by it (keypresses, analog values, etc)
	 * Note: This can produce A LOT of logs, so only use this if you are debugging something.
	 */
	UPROPERTY(config, EditAnywhere, Category = Logging, meta=(ConsoleVariable="EnhancedEditorInput.bShouldLogAllInputs"))
	uint8 bLogAllInput : 1;
	
	/** If true, then the UEnhancedInputEditorSubsystem will be started when it is initalized */
	UPROPERTY(config, EditAnywhere, Category = Editor, meta=(ConsoleVariable="EnhancedEditorInput.bAutomaticallyStartConsumingInput"))
	uint8 bAutomaticallyStartConsumingInput : 1;

	/** A bitmask of what event pins are visible when you place an Input Action event node in blueprints.  */
	UPROPERTY(config, EditAnywhere, Category = Blueprints, meta = (Bitmask, BitmaskEnum = "/Script/EnhancedInput.ETriggerEvent"))
	uint8 VisibleEventPinsByDefault;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "EnhancedInputDeveloperSettings.h"
#endif
