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
		case EVCamInputLoggingMode::AllExceptMouse:
			if (!Params.Key.IsMouseButton())
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

static TAutoConsoleVariable<bool> CVarSkipMouseAxisInput(
	TEXT("VCam.SkipMouseAxisInput"),
	false,
	TEXT("Whether VCam input should skip processing mouse axis events. This is useful for setting break points in UVCamPlayerInput::InputKey, which would get spammed by mouse axis events otherwise."),
	ECVF_Default
	);

bool UVCamPlayerInput::InputKey(const FInputKeyParams& Params)
{
	const bool bIsMouseAxis = Params.Key == EKeys::MouseX || Params.Key == EKeys::MouseY || Params.Key == EKeys::Mouse2D;
	if (bIsMouseAxis && CVarSkipMouseAxisInput.GetValueOnGameThread())
	{
		return false;
	}
	
	const bool bIsKeyboard = !Params.IsGamepad() && !Params.Key.IsAnalog() && !Params.Key.IsMouseButton() && !Params.Key.IsTouch()
		// Keyboard is always mapped to 0
		&& Params.InputDevice.GetId() == 0;
	const bool bCanCheckAllowList = Params.InputDevice != INPUTDEVICEID_NONE && !bIsKeyboard;
	
	const bool bSkipGamepad = Params.IsGamepad() && InputDeviceSettings.GamepadInputMode != EVCamGamepadInputMode::Allow && InputDeviceSettings.GamepadInputMode != EVCamGamepadInputMode::AllowAndConsume;
	const bool bSkipMouse = InputDeviceSettings.MouseInputMode == EVCamInputMode::Ignore && Params.Key.IsMouseButton();
	const bool bSkipKeyboard = InputDeviceSettings.KeyboardInputMode == EVCamInputMode::Ignore && bIsKeyboard;
	const bool bSkipNonAllowListed = bCanCheckAllowList && !InputDeviceSettings.bAllowAllInputDevices && !InputDeviceSettings.AllowedInputDeviceIds.Contains(FVCamInputDeviceID{ Params.InputDevice.GetId() });
	const bool bIsFilteredOut = bSkipGamepad || bSkipMouse || bSkipKeyboard || bSkipNonAllowListed;
	
	UE::VCamCore::Private::LogInput(InputDeviceSettings, Params, bIsFilteredOut);
	if (!bIsFilteredOut)
	{
		const bool bForceConsumeGamepad = Params.IsGamepad() && InputDeviceSettings.GamepadInputMode == EVCamGamepadInputMode::AllowAndConsume;
		return Super::InputKey(Params) || bForceConsumeGamepad;
	}

	const bool bConsumeGamepad = !bSkipNonAllowListed && Params.IsGamepad() && InputDeviceSettings.GamepadInputMode == EVCamGamepadInputMode::IgnoreAndConsume;
	return bConsumeGamepad;
}

void UVCamPlayerInput::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);
}

void UVCamPlayerInput::SetInputSettings(const FVCamInputDeviceConfig& Input)
{
	InputDeviceSettings = Input;
}
