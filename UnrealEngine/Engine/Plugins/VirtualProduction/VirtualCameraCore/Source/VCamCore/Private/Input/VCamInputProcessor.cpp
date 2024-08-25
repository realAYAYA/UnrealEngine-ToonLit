// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputProcessor.h"

#include "Input/InputVCamSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "Input/Events.h"

namespace UE::VCamCore::Private
{
	FVCamInputProcessor::FVCamInputProcessor(UInputVCamSubsystem& OwningSubsystem)
		: OwningSubsystem(&OwningSubsystem)
	{}

	void FVCamInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
	{
		ProcessAccumulatedPointerInput(DeltaTime);
	}

	bool FVCamInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = InKeyEvent.GetKey();
		Params.Event = IE_Pressed;
		Params.Delta.X = 1.0;
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
		Params.InputDevice = InKeyEvent.GetInputDeviceId();

		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = InKeyEvent.GetKey();
		Params.Event = IE_Released;
		Params.Delta.X = 0.0;
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
		Params.InputDevice = InKeyEvent.GetInputDeviceId();
		
		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = InAnalogInputEvent.GetKey();
		Params.Event = IE_Pressed;
		Params.Delta.X = InAnalogInputEvent.GetAnalogValue();
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = 1;
		Params.InputDevice = InAnalogInputEvent.GetInputDeviceId();
		
		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		UpdateCachedPointerPosition(MouseEvent);
		return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent);
	}

	bool FVCamInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = MouseEvent.GetEffectingButton();
		Params.Event = IE_Pressed;
		Params.Delta.X = 1.0;
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = 0;

		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = MouseEvent.GetEffectingButton();
		Params.Event = IE_Released;
		Params.Delta.X = 0.0;
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = 0;
		
		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyParams Params = {};
		Params.Key = MouseEvent.GetEffectingButton();
		Params.Event = IE_DoubleClick;
		Params.Delta.X = 1.0;
		Params.DeltaTime = SlateApp.GetDeltaTime();
		Params.NumSamples = 0;
		
		return InputKeyToSubsystem(Params);
	}

	bool FVCamInputProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
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
			
			InputKeyToSubsystem(Params);
		}
		
		return IInputProcessor::HandleMouseWheelOrGestureEvent(SlateApp, InWheelEvent, InGestureEvent);
	}

	void FVCamInputProcessor::UpdateCachedPointerPosition(const FPointerEvent& MouseEvent)
	{
		CachedCursorDelta = MouseEvent.GetCursorDelta();
		
		++NumCursorSamplesThisFrame.X;
		++NumCursorSamplesThisFrame.Y;
	}

	void FVCamInputProcessor::ProcessAccumulatedPointerInput(float DeltaTime)
	{
		// Input the MouseX value
		{
			FInputKeyParams Params = {};
			Params.Key = EKeys::MouseX;
			Params.Delta.X = CachedCursorDelta.X;
			Params.DeltaTime = DeltaTime;
			Params.NumSamples = NumCursorSamplesThisFrame.X;
			InputKeyToSubsystem(Params);
		}

		// Input the MouseY value
		{
			FInputKeyParams Params = {};
			Params.Key = EKeys::MouseY;
			Params.Delta.X = CachedCursorDelta.Y;
			Params.DeltaTime = DeltaTime;
			Params.NumSamples = NumCursorSamplesThisFrame.Y;
			InputKeyToSubsystem(Params);
		}
		
		NumCursorSamplesThisFrame = FIntPoint::ZeroValue;
		CachedCursorDelta = FVector2D::ZeroVector;
	}

	bool FVCamInputProcessor::InputKeyToSubsystem(const FInputKeyParams& Params)
	{
		// Even after our owning subsystem is destroyed, the core input system may hold onto us for just a little bit longer due to how the input system is designed
		if (OwningSubsystem.IsValid())
		{
			return OwningSubsystem->InputKey(Params);
		}
		
		return false;
	}
}
