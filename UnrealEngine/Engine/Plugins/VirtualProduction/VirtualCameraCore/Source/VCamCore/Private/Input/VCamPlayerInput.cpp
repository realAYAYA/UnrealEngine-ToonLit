// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/VCamPlayerInput.h"

#include "LogVCamCore.h"

#include "HAL/IConsoleManager.h"

namespace UE::VCamCore::Private
{
	static FString LexInputEvent(EInputEvent InputEvent)
	{
		switch (InputEvent)
		{
		case IE_Pressed: return TEXT("pressed");
		case IE_Released: return TEXT("released");
		case IE_Repeat: return TEXT("repeat");
		case IE_DoubleClick: return TEXT("double-click");
		case IE_Axis: return TEXT("axis");
		case IE_MAX: // pass-through
		default: return TEXT("invalid");
		}
	}
	
	static FString ToString(const FInputKeyParams& Params)
	{
		return FString::Printf(TEXT("{ InputID: %d, Key: %s, EInputEvent: %s, bIsGamepad: %s }"),
			Params.InputDevice.GetId(),
			*Params.Key.ToString(),
			*LexInputEvent(Params.Event),
			Params.IsGamepad() ? TEXT("true") : TEXT("false")
			);
	}

	static void LogInput(FVCamInputDeviceConfig InputDeviceSettings, const FInputKeyParams& Params, bool bIsFilteredOut)
	{
		switch (InputDeviceSettings.LoggingMode)
		{
		case EVCamInputLoggingMode::OnlyConsumable:
			if (!bIsFilteredOut)
			{
				UE_LOG(LogVCamInputDebug, Log, TEXT("%s"), *ToString(Params));
			}
			break;
		case EVCamInputLoggingMode::OnlyGamepad:
			if (Params.IsGamepad())
			{
				UE_LOG(LogVCamInputDebug, Log, TEXT("%s"), *ToString(Params));
			}
			break;
		
		case EVCamInputLoggingMode::All:
			UE_LOG(LogVCamInputDebug, Log, TEXT("%s"), *ToString(Params));
		default: ;
		}
	}
}

bool UVCamPlayerInput::InputKey(const FInputKeyParams& Params)
{
	// VCam will never allow mouse
	const bool bIsMouseAxis = Params.Key == EKeys::MouseX || Params.Key == EKeys::MouseY || Params.Key == EKeys::Mouse2D;
	const bool bIsMouse = bIsMouseAxis || Params.Key.IsMouseButton();
	if (bIsMouse)
	{
		return false;
	}
	
	const bool bIsKeyboard = !Params.IsGamepad() && !Params.Key.IsAnalog() && !Params.Key.IsTouch()
		// Keyboard is always mapped to 0
		&& Params.InputDevice.GetId() == 0;
	const bool bCanCheckAllowList = Params.InputDevice != INPUTDEVICEID_NONE && !bIsKeyboard;
	const bool bSkipNonAllowListed = bCanCheckAllowList && !InputDeviceSettings.bAllowAllInputDevices && !InputDeviceSettings.AllowedInputDeviceIds.Contains(FVCamInputDeviceID{ Params.InputDevice.GetId() });
	const bool bIsFilteredOut = bSkipNonAllowListed || InputDeviceSettings.InputMode == EVCamInputMode::Ignore;
	
	UE::VCamCore::Private::LogInput(InputDeviceSettings, Params, bIsFilteredOut);
	if (!bIsFilteredOut)
	{
		const bool bCanConsumeInput = Super::InputKey(Params)
			// For some reason, when the key is released the input code skips calling IsKeyHandledByAction and always returns true instead. We need to check it ourselves:
			&& IsKeyHandledByAction(Params.Key);

		// Gamepads either 1. always consume in ConsumeDevice mode or 2. consume if in ConsumeIfUsed mode and InputKey returned true.
		const bool bConsumeGamepad = Params.IsGamepad()
			&& (InputDeviceSettings.InputMode == EVCamInputMode::ConsumeDevice
				|| (InputDeviceSettings.InputMode == EVCamInputMode::ConsumeIfUsed && bCanConsumeInput));
		// Keyboards only consume if InputKey returned true and if either in ConsumeDevice or ConsumeIfUsed mode
		const bool bConsumeKeyboard = bIsKeyboard && bCanConsumeInput && (InputDeviceSettings.InputMode == EVCamInputMode::ConsumeDevice || InputDeviceSettings.InputMode == EVCamInputMode::ConsumeIfUsed);
		
		const bool bConsumeInput = bConsumeGamepad || bConsumeKeyboard;
		return bConsumeInput;
	}

	// Allowed gamepads always consume if in ConsumeDevice mode.
	const bool bConsumeGamepad = !bSkipNonAllowListed && Params.IsGamepad() && InputDeviceSettings.InputMode == EVCamInputMode::ConsumeDevice;
	return bConsumeGamepad;
}

void UVCamPlayerInput::SetInputSettings(const FVCamInputDeviceConfig& Input)
{
	InputDeviceSettings = Input;
}
