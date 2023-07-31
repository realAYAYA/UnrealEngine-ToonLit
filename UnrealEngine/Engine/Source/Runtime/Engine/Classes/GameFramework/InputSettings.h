// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerInput.h"
#include "Components/InputComponent.h"

#include "InputSettings.generated.h"

/**
 * Project wide settings for input handling
 * 
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Input/index.html
 */
UCLASS(config=Input, defaultconfig)
class ENGINE_API UInputSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Properties of Axis controls */
	UPROPERTY(config, EditAnywhere, EditFixedSize, Category="Bindings", meta=(ToolTip="List of Axis Properties"), AdvancedDisplay)
	TArray<struct FInputAxisConfigEntry> AxisConfig;

	UPROPERTY(config, EditAnywhere, Category="Bindings", AdvancedDisplay)
	uint8 bAltEnterTogglesFullscreen:1;

	UPROPERTY(config, EditAnywhere, Category = "Bindings", AdvancedDisplay)
	uint8 bF11TogglesFullscreen : 1;

	// Allow mouse to be used for touch
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint8 bUseMouseForTouch:1;

	// Mouse smoothing control
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint8 bEnableMouseSmoothing:1;

	// Scale the mouse based on the player camera manager's field of view
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint8 bEnableFOVScaling:1;

	/** Controls if the viewport will capture the mouse on Launch of the application */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	uint8 bCaptureMouseOnLaunch:1;

	/** Enable the use of legacy input scales on the player controller (InputYawScale, InputPitchScale, and InputRollScale) */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	uint8 bEnableLegacyInputScales:1;
	
	/**
	 * If set to false, then the player controller's InputMotion function will never be called.
	 * This will effectively disable any motion input (tilt, rotation, acceleration, etc) on
	 * the GameViewportClient.
	 * 
	 * @see GameViewportClient::InputMotion
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	uint8 bEnableMotionControls:1;

	/**
	 * If true, then the PlayerController::InputKey function will only process an input event if it
	 * came from an input device that is owned by the PlayerController's Platform User.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	uint8 bFilterInputByPlatformUser:1;

	/**
	 * If true, then the Player Controller will have it's Pressed Keys flushed when the input mode is changed
	 * to Game and UI mode or the game viewport loses focus. The default behavior is true.
	 * 
	 * @see UGameViewportClient::LostFocus
	 * @see APlayerController::ShouldFlushKeysWhenViewportFocusChanges
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	uint8 bShouldFlushPressedKeysOnViewportFocusLost:1;

	/**
	 * Should components that are dynamically added via the 'AddComponent' function at runtime have input delegates bound to them?
	 * @see AActor::FinishAddComponent
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	uint8 bEnableDynamicComponentInputBinding:1;
	
	/** Should the touch input interface be shown always, or only when the platform has a touch screen? */
	UPROPERTY(config, EditAnywhere, Category="Mobile")
	uint8 bAlwaysShowTouchInterface:1;

	/** Whether or not to show the console on 4 finger tap, on mobile platforms */
	UPROPERTY(config, EditAnywhere, Category="Mobile")
	uint8 bShowConsoleOnFourFingerTap:1;

	/** Whether or not to use the gesture recognition system to convert touches in to gestures that can be bound and queried */
	UPROPERTY(config, EditAnywhere, Category = "Mobile")
	uint8 bEnableGestureRecognizer:1;

	/** If enabled, virtual keyboards will have autocorrect enabled. Currently only supported on mobile devices. */
	UPROPERTY(config, EditAnywhere, Category = "Virtual Keyboard (Mobile)")
	uint8 bUseAutocorrect:1;

	/** 
	 * Disables autocorrect for these operating systems, even if autocorrect is enabled. Use the format "[platform] [osversion]"
	 * (e.g., "iOS 11.2" or "Android 6"). More specific versions will disable autocorrect for fewer devices ("iOS 11" will disable
	 * autocorrect for all devices running iOS 11, but "iOS 11.2.2" will not disable autocorrect for devices running 11.2.1).
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Virtual Keyboard (Mobile)")
	TArray<FString> ExcludedAutocorrectOS;

	/** Disables autocorrect for these cultures, even if autocorrect is turned on. These should be ISO-compliant language and country codes, such as "en" or "en-US". */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Virtual Keyboard (Mobile)")
	TArray<FString> ExcludedAutocorrectCultures;

	/** 
	 * Disables autocorrect for these device models, even if autocorrect is turned in. Model IDs listed here will match against the start of the device's
	 * model (e.g., "SM-" will match all device model IDs that start with "SM-"). This is currently only supported on Android devices.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Virtual Keyboard (Mobile)")
	TArray<FString> ExcludedAutocorrectDeviceModels;

	/** The default mouse capture mode for the game viewport */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	EMouseCaptureMode DefaultViewportMouseCaptureMode;

	/** The default mouse lock state behavior when the viewport acquires capture */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	EMouseLockMode DefaultViewportMouseLockMode;

	// The scaling value to multiply the field of view by
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay, meta=(editcondition="bEnableFOVScaling"))
	float FOVScale;

	/** If a key is pressed twice in this amount of time it is considered a "double click" */
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	float DoubleClickTime;

private:
	/** List of Action Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputActionKeyMapping> ActionMappings;

	/** List of Axis Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputAxisKeyMapping> AxisMappings;

	/** List of Speech Mappings */
	UPROPERTY(config, EditAnywhere, Category = "Bindings")
	TArray<struct FInputActionSpeechMapping> SpeechMappings;

	/** Default class type for player input object. May be overridden by player controller. */
	UPROPERTY(config, EditAnywhere, NoClear, Category = DefaultClasses)
	TSoftClassPtr<UPlayerInput> DefaultPlayerInputClass;

	/** Default class type for pawn input components. */
	UPROPERTY(config, EditAnywhere, NoClear, Category = DefaultClasses)
	TSoftClassPtr<UInputComponent> DefaultInputComponentClass;

public:
	/** The default on-screen touch input interface for the game (can be null to disable the onscreen interface) */
	UPROPERTY(config, EditAnywhere, Category="Mobile", meta=(AllowedClasses="/Script/Engine.TouchInterface"))
	FSoftObjectPath DefaultTouchInterface;

	/** The keys which open the console. */
	UPROPERTY(config, EditAnywhere, Category="Console")
	TArray<FKey> ConsoleKeys;

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostReloadConfig( class FProperty* PropertyThatWasLoaded ) override;
#endif

	void RemoveInvalidKeys();

	virtual void PostInitProperties() override;
	// End of UObject interface

	/** Returns the game local input settings (action mappings, axis mappings, etc...) */
	UFUNCTION(BlueprintPure, Category = Settings)
	static UInputSettings* GetInputSettings();

	/** Programmatically add an action mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void AddActionMapping(const FInputActionKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	UFUNCTION(BlueprintPure, Category = Settings)
	void GetActionMappingByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const;

	/** Programmatically remove an action mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void RemoveActionMapping(const FInputActionKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Programmatically add an axis mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void AddAxisMapping(const FInputAxisKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Retrieve all axis mappings by a certain name. */
	UFUNCTION(BlueprintPure, Category = Settings)
	void GetAxisMappingByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const;

	/** Programmatically remove an axis mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void RemoveAxisMapping(const FInputAxisKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Flush the current mapping values to the config file */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SaveKeyMappings();

	/** Populate a list of all defined action names */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void GetActionNames(TArray<FName>& ActionNames) const;

	/** Populate a list of all defined axis names */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void GetAxisNames(TArray<FName>& AxisNames) const;

	/** When changes are made to the default mappings, push those changes out to PlayerInput key maps */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void ForceRebuildKeymaps();

	/** Finds unique action name based on existing action names */
	FName GetUniqueActionName(const FName BaseActionMappingName);
	/** Finds unique axis name based on existing action names */
	FName GetUniqueAxisName(const FName BaseAxisMappingName);

	/** Append new mapping to existing list */
	void AddActionMapping(FInputActionKeyMapping& NewMapping);
	/** Append new mapping to existing list */
	void AddAxisMapping(FInputAxisKeyMapping& NewMapping);

	/** Ask for all the action mappings */
	const TArray <FInputActionKeyMapping>& GetActionMappings() const;
	/** Ask for all the axis mappings */
	const TArray <FInputAxisKeyMapping>& GetAxisMappings() const;
	/** Ask for all the speech mappings */
	const TArray <FInputActionSpeechMapping>& GetSpeechMappings() const;

	/** Finds unique action name based on existing action names */
	bool DoesActionExist(const FName InActionName);
	/** Finds unique axis name based on existing action names */
	bool DoesAxisExist(const FName InAxisName);
	/** Finds unique speech name based on existing speech names */
	bool DoesSpeechExist(const FName InSpeechName);


	/** Get the member name for the details panel */
	static const FName GetActionMappingsPropertyName();
	/** Get the member name for the details panel */
	static const FName GetAxisMappingsPropertyName();

	// Class accessors
	static UClass* GetDefaultPlayerInputClass();
	static UClass* GetDefaultInputComponentClass();
	
	/**
	 * Set the default player input class.
	 *
	 * @param NewDefaultPlayerInputClass The new class to use.
	 */
	static void SetDefaultPlayerInputClass(TSubclassOf<UPlayerInput> NewDefaultPlayerInputClass);

	/**
	 * Set the default input component class.
	 *
	 * @param NewDefaultInputComponentClass The new class to use.
	 */
	static void SetDefaultInputComponentClass(TSubclassOf<UInputComponent> NewDefaultInputComponentClass);
	
private:
	void PopulateAxisConfigs();
};
