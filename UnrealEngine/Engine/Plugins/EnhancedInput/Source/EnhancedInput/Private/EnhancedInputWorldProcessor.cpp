// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputWorldProcessor.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "EnhancedInputDeveloperSettings.h"
#include "UObject/UObjectIterator.h"

void FEnhancedInputWorldProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	ProcessAccumulatedPointerInput(DeltaTime);
}

bool FEnhancedInputWorldProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InKeyEvent.GetKey();
	Params.Event = IE_Pressed;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
	Params.InputDevice = InKeyEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	
	return IInputProcessor::HandleKeyDownEvent(SlateApp, InKeyEvent);
}

bool FEnhancedInputWorldProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InKeyEvent.GetKey();
	Params.Event = IE_Released;
	Params.Delta.X = 0.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
	Params.InputDevice = InKeyEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleKeyUpEvent(SlateApp, InKeyEvent);
}

bool FEnhancedInputWorldProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InAnalogInputEvent.GetKey();
	Params.Event = IE_Pressed;
	Params.Delta.X = InAnalogInputEvent.GetAnalogValue();
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 1;
	Params.InputDevice = InAnalogInputEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleAnalogInputEvent(SlateApp, InAnalogInputEvent);
}

bool FEnhancedInputWorldProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	UpdateCachedPointerPosition(MouseEvent);
	
	return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputWorldProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_Pressed;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	Params.InputDevice = MouseEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonDownEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputWorldProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_Released;
	Params.Delta.X = 0.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	Params.InputDevice = MouseEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonUpEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputWorldProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_DoubleClick;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	Params.InputDevice = MouseEvent.GetInputDeviceId();
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonDoubleClickEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputWorldProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	const FKey MouseWheelKey = InWheelEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;

	// Input the Mouse wheel key events (mouse scroll down or scroll up) as being pressed and released this frame
	// The SceneViewport Inputs the Mouse Scroll Wheel buttons up and down in the same frame, this replicates that behavior
	{
		FInputKeyParams PressedParams = {};
		PressedParams.Key = MouseWheelKey;
		PressedParams.Event = IE_Pressed;
		PressedParams.Delta.X = 1.0;
		PressedParams.DeltaTime = SlateApp.GetDeltaTime();
		PressedParams.NumSamples = 0;
		PressedParams.InputDevice = InWheelEvent.GetInputDeviceId();
		
		FInputKeyParams ReleasedParams = PressedParams;
		ReleasedParams.Event = IE_Released;

		InputKeyToSubsystem(PressedParams);
		InputKeyToSubsystem(ReleasedParams);	
	}
	// Input the wheel axis delta to get the MouseWheelAxis button working
	{
		FInputKeyParams Params = {};
		Params.Key = EKeys::MouseWheelAxis;
		Params.Delta.X = InWheelEvent.GetWheelDelta();
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = 1;
		Params.InputDevice = InWheelEvent.GetInputDeviceId();
		
		InputKeyToSubsystem(Params);
	}
	
	return IInputProcessor::HandleMouseWheelOrGestureEvent(SlateApp, InWheelEvent, InGestureEvent);
}

void FEnhancedInputWorldProcessor::UpdateCachedPointerPosition(const FPointerEvent& MouseEvent)
{
	CachedCursorDelta = MouseEvent.GetCursorDelta();
	
	++NumCursorSamplesThisFrame.X;
	++NumCursorSamplesThisFrame.Y;
}

void FEnhancedInputWorldProcessor::ProcessAccumulatedPointerInput(float DeltaTime)
{
	// Input the MouseX value
	{
		FInputKeyParams Params = {};
		Params.Key = EKeys::MouseX;
		Params.Delta.X = CachedCursorDelta.X;
		Params.DeltaTime = DeltaTime;
		Params.NumSamples = NumCursorSamplesThisFrame.X;
		Params.InputDevice = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();

		InputKeyToSubsystem(Params);
	}

	// Input the MouseY value
	{
		FInputKeyParams Params = {};
		Params.Key = EKeys::MouseY;
		Params.Delta.X = CachedCursorDelta.Y;
		Params.DeltaTime = DeltaTime;
		Params.NumSamples = NumCursorSamplesThisFrame.Y;
		Params.InputDevice = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();

		InputKeyToSubsystem(Params);
	}
	
	NumCursorSamplesThisFrame = FIntPoint::ZeroValue;
	CachedCursorDelta = FVector2D::ZeroVector;
}


bool FEnhancedInputWorldProcessor::InputKeyToSubsystem(const FInputKeyParams& Params)
{
	bool bRes = false;
	
	if (GetDefault<UEnhancedInputDeveloperSettings>()->bEnableWorldSubsystem)
	{
		// Tell all the world subsystems about the key that has been pressed
		for (TObjectIterator<UEnhancedInputWorldSubsystem> It; It; ++It)
		{
			bRes |= (*It)->InputKey(Params);
		}
	}
	
	return bRes;
}
