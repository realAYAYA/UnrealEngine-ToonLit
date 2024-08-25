// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputDeviceDetectionProcessor.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Misc/CoreMiscDefines.h"
#include "Templates/UnrealTemplate.h"

namespace UE::VCamCoreEditor::Private
{
	bool IsEmulatedAnalogStickPressOrReleaseKey(const FKeyEvent& InKeyEvent)
	{
		const FKey Key = InKeyEvent.GetKey();
		const TSet<FKey> EmulatedSticks {
			EKeys::Gamepad_LeftStick_Down,
			EKeys::Gamepad_LeftStick_Up,
			EKeys::Gamepad_LeftStick_Right,
			EKeys::Gamepad_LeftStick_Left,
			EKeys::Gamepad_RightStick_Up,
			EKeys::Gamepad_RightStick_Down,
			EKeys::Gamepad_RightStick_Right,
			EKeys::Gamepad_RightStick_Left
		};
		return EmulatedSticks.Contains(InKeyEvent.GetKey());
	}
	
	TSharedPtr<FInputDeviceDetectionProcessor> FInputDeviceDetectionProcessor::MakeAndRegister(FOnInputDeviceDetected OnInputDeviceDetectedDelegate, FInputDeviceSelectionSettings Settings)
	{
		if (ensure(FSlateApplication::IsInitialized()))
		{
			TSharedRef<FInputDeviceDetectionProcessor> Result = MakeShared<FInputDeviceDetectionProcessor>(MoveTemp(OnInputDeviceDetectedDelegate), Settings);
			FSlateApplication::Get().RegisterInputPreProcessor(Result, 0);
			return Result;
		}
		return nullptr;
	}

	void FInputDeviceDetectionProcessor::Unregister()
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(SharedThis(this));
		}
	}

	void FInputDeviceDetectionProcessor::UpdateInputSettings(FInputDeviceSelectionSettings InSettings)
	{
		Settings = InSettings;
	}

	FInputDeviceDetectionProcessor::FInputDeviceDetectionProcessor(FOnInputDeviceDetected OnInputDeviceDetectedDelegate, FInputDeviceSelectionSettings Settings)
		: OnInputDeviceDetectedDelegate(MoveTemp(OnInputDeviceDetectedDelegate))
		, Settings(Settings)
	{}

	bool FInputDeviceDetectionProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		// One might think that HandleAnalogInputEvent would handle all analog events but if you move the thumbstick, you'll get pressed / release events, too.
		if (!IsEmulatedAnalogStickPressOrReleaseKey(InKeyEvent) || Settings.bAllowAnalog)
		{
			OnInputDeviceDetectedDelegate.Execute(InKeyEvent.GetInputDeviceId().GetId());
		}
		return true;
	}

	bool FInputDeviceDetectionProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		// One might think that HandleAnalogInputEvent would handle all analog events but if you move the thumbstick, you'll get pressed / release events, too.
		if (!IsEmulatedAnalogStickPressOrReleaseKey(InKeyEvent) || Settings.bAllowAnalog)
		{
			OnInputDeviceDetectedDelegate.Execute(InKeyEvent.GetInputDeviceId().GetId());
		}
		return true;
	}

	bool FInputDeviceDetectionProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
	{
		if (Settings.bAllowAnalog)
		{
			OnInputDeviceDetectedDelegate.Execute(InAnalogInputEvent.GetInputDeviceId().GetId());
		}
		return true;
	}
}
