// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Engine/PlatformSettings.h"
#include "EnhancedInputDeveloperSettings.generated.h"

/** Developer settings for Enhanced Input */
UCLASS(config = Input, defaultconfig, meta = (DisplayName = "Enhanced Input"))
class ENHANCEDINPUT_API UEnhancedInputDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:
	
	UEnhancedInputDeveloperSettings(const FObjectInitializer& Initializer);

	/**
	 * Platform specific settings for Enhanced Input.
	 * @see UEnhancedInputPlatformSettings
	 */
	UPROPERTY(EditAnywhere, Category = "Enhanced Input")
	FPerPlatformSettings PlatformSettings;

	/**
	 * If true, then only the last action in a ChordedAction trigger will be fired.
	 * This means that only the action that has the ChordedTrigger on it will be fired, not the individual steps.
	 * 
	 * Default value is true.
	 */
	UPROPERTY(EditAnywhere, Category = "Enhanced Input", meta=(ConsoleVariable="EnhancedInput.OnlyTriggerLastActionInChord"))
	bool bShouldOnlyTriggerLastActionInChord = true;
};