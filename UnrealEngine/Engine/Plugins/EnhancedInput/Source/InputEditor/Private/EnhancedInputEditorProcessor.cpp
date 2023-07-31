// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputEditorProcessor.h"
#include "EnhancedInputEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"		// For GEditor
#include "GameFramework/PlayerInput.h"

void FEnhancedInputEditorProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	ProcessAccumulatedPointerInput(DeltaTime);
}

bool FEnhancedInputEditorProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InKeyEvent.GetKey();
	Params.Event = IE_Pressed;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
	
	InputKeyToSubsystem(Params);
	
	return IInputProcessor::HandleKeyDownEvent(SlateApp, InKeyEvent);
}

bool FEnhancedInputEditorProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InKeyEvent.GetKey();
	Params.Event = IE_Released;
	Params.Delta.X = 0.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = Params.Key.IsAnalog() ? 1 : 0;
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleKeyUpEvent(SlateApp, InKeyEvent);
}

bool FEnhancedInputEditorProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	FInputKeyParams Params = {};
	Params.Key = InAnalogInputEvent.GetKey();
	Params.Event = IE_Pressed;
	Params.Delta.X = InAnalogInputEvent.GetAnalogValue();
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 1;
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleAnalogInputEvent(SlateApp, InAnalogInputEvent);
}

bool FEnhancedInputEditorProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	UpdateCachedPointerPosition(MouseEvent);
	
	return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputEditorProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_Pressed;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonDownEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputEditorProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_Released;
	Params.Delta.X = 0.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonUpEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputEditorProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FInputKeyParams Params = {};
	Params.Key = MouseEvent.GetEffectingButton();
	Params.Event = IE_DoubleClick;
	Params.Delta.X = 1.0;
	Params.DeltaTime = SlateApp.GetDeltaTime();
	Params.NumSamples = 0;
	
	InputKeyToSubsystem(Params);
	return IInputProcessor::HandleMouseButtonDoubleClickEvent(SlateApp, MouseEvent);
}

bool FEnhancedInputEditorProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
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

void FEnhancedInputEditorProcessor::UpdateCachedPointerPosition(const FPointerEvent& MouseEvent)
{
	CachedCursorDelta = MouseEvent.GetCursorDelta();
	
	++NumCursorSamplesThisFrame.X;
	++NumCursorSamplesThisFrame.Y;
}

void FEnhancedInputEditorProcessor::ProcessAccumulatedPointerInput(float DeltaTime)
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

bool FEnhancedInputEditorProcessor::InputKeyToSubsystem(const FInputKeyParams& Params)
{
	// TODO: We may want to process the slate user ID that this input key came from in the future
	// but for now we don't need to worry about it.
	
	// GEditor should always be valid here since the preprocessor is spawned by an Editor Subsystem
	if (ensure(GEditor))
	{
		if (UEnhancedInputEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>())
		{
			if (Subsystem->GetWorld())
			{
				return Subsystem->InputKey(Params);
			}
		}	
	}
	
	return false;
}
