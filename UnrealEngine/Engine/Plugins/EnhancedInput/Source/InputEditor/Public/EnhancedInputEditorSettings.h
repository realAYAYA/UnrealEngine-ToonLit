// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "EnhancedInputEditorSettings.generated.h"

class UEnhancedPlayerInput;
class UInputMappingContext;
class UEnhancedInputEditorSubsystem;

USTRUCT()
struct FDefaultContextSetting
{
	GENERATED_BODY()

	/** Input Mapping Context that should be Added to the EnhancedInputEditorSubsystem when it starts listening for input */
	UPROPERTY(EditAnywhere, Category = "Input")
	TSoftObjectPtr<const UInputMappingContext> InputMappingContext = nullptr;

	/** The prioirty that should be given to this mapping context when it is added */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;
};

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
 * A collection of useful indivudal user settings when using the EnhancedInputEditorSubsystem.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Enhanced Input Editor Settings"))
class UEnhancedInputEditorSettings : public UDeveloperSettingsBackedByCVars
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
};