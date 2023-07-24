// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamInputDeviceConfig.generated.h"

UENUM()
enum class EVCamInputLoggingMode : uint8
{
	/** No logging */
	None,
	
	/** Log only input that are passed down to the input actions (i.e. that passed filtering conditions) */
	OnlyConsumable,

	/** Log all gamepad input regardless whether it is passed down to input actions or not (i.e. that passed filtering conditions) */
	OnlyGamepad,
	
	/** Log all input, regardless whether it is passed down to input actions or not (i.e. that passed filtering conditions). */
	All,
	
	/** Like All but never reports mouse data (generated every tick - it could spam the output log). */
	AllExceptMouse
};

UENUM()
enum class EVCamInputMode : uint8
{
	/** Input is forwarded to input actions but not consumed. */
	Allow,
	/** Input is not forwarded to input actions. */
	Ignore
};

UENUM()
enum class EVCamGamepadInputMode : uint8
{
	/** Input is forwarded to input actions but not consumed. */
	Allow,
	/** Input is forwarded to input actions and is ALWAYS consumed, regardless of whether an action was bound to it or not.*/
	AllowAndConsume,
	/** Input is not forwarded to input actions. */
	Ignore,
	/** Input is not forwarded to input actions and we block anybody else from receiving this input. */
	IgnoreAndConsume
};

USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamInputDeviceID
{
	GENERATED_BODY()

	/**
	 * The ID of an input device.
	 *
	 * Input device IDs start at 0 and increase by 1 as more devices connect. When a device disconnects, the ID is recycled
	 * and becomes available for reassignment to the next device that connects; when a device connects, the lowest possible ID is reassigned.
	 *
	 * Example: suppose you have three gamepads called A, B, and VCamDevicePairingConfig.h
	 * 1. Connect gamepad A > receives ID 0
	 * 2. Connect gamepad B > receives ID 1
	 * 3. Disconnect gamepad A > gamepad B will still have ID 1
	 * 4. Connect the same gamepad A OR another gamepad C > receives ID 0.
	 *
	 * Note: Keyboards always have ID = 0, mice ID = -1.
	 * Note: The first gamepad will have ID = 0 even though keyboards will also have ID 0.
	 * 
	 * Default value of -10 means no input device. There is nothing special about -10 (-1 already used by mice).
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 DeviceId = -10;

	friend bool operator==(const FVCamInputDeviceID& Left, const FVCamInputDeviceID& Right) { return Left.DeviceId == Right.DeviceId; }
	friend bool operator!=(const FVCamInputDeviceID& Left, const FVCamInputDeviceID& Right) { return !(Left == Right); }
};

/** Defines the input devices a UVCamComponent will accept input from. */
USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamInputDeviceConfig
{
	GENERATED_BODY()

	/**
	 * Determines how input devices are filtered:
	 * True: Every device is allowed.
	 * False: Only input from devices with the IDs in AllowedInputDeviceIds is allowed.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bAllowAllInputDevices = true;
	
	/**
	 * List of input devices from which input can trigger input actions.
	 * Typically this is used for gamepads.
	 *
	 * Input device IDs start at 0 and increase by 1 as more devices connect. When a device disconnects, the ID is recycled
	 * and becomes available for reassignment to the next device that connects; when a device connects, the lowest possible ID is reassigned.
	 *
	 * Example: suppose you have three gamepads called A, B, and VCamDevicePairingConfig.h
	 * 1. Connect gamepad A > receives ID 0
	 * 2. Connect gamepad B > receives ID 1
	 * 3. Disconnect gamepad A > gamepad B will still have ID 1
	 * 4. Connect the same gamepad A OR another gamepad C > receives ID 0.
	 *
	 * Note: Keyboards always have ID = 0, mice ID = -1.
	 * Note: The first gamepad will have ID = 0 even though keyboards will also have ID 0.
	 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (EditCondition = "!bAllowAllInputDevices", EditConditionHides))
	TArray<FVCamInputDeviceID> AllowedInputDeviceIds;
	
	/**
	 * Should keyboard input trigger input actions?
	 * Keyboards are always mapped to input device ID 0.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	EVCamInputMode KeyboardInputMode = EVCamInputMode::Allow;

	/**
	 * Should mouse input trigger input actions?
	 * Mice are always mapped to input device ID -1.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	EVCamInputMode MouseInputMode = EVCamInputMode::Allow;

	/** What should be done with gamepad input. */
	UPROPERTY(EditAnywhere, Category = "Input", AdvancedDisplay)
	EVCamGamepadInputMode GamepadInputMode = EVCamGamepadInputMode::Allow;

	/**
	 * What type of input should be logged.
	 *
	 * Tip: Filter the log by LogVCamInputDebug.
	 */
	UPROPERTY(EditAnywhere, Category = "Input", AdvancedDisplay)
	EVCamInputLoggingMode LoggingMode = EVCamInputLoggingMode::None;
};