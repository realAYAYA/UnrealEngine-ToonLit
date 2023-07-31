// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VCamInputTypes.h"
#include "Framework/Application/IInputProcessor.h"

class FVCamInputProcessor : public IInputProcessor
{
public:
	// Input Processor Interface

    /** Input Tick */
    virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;

	/** Key down input */
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	/** Key up input */
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	/** Analog axis input */
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;

	/** Mouse movement input */
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/** Mouse button press */
	virtual bool HandleMouseButtonDownEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/** Mouse button release */
	virtual bool HandleMouseButtonUpEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/** Mouse button double clicked. */
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/** Mouse wheel input */
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;

	/** Called when a motion-driven device has new input */
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override;

	// End Input Processor Interface

	TInputDelegateStore<FKeyInputDelegate> KeyDownDelegateStore;
	TInputDelegateStore<FKeyInputDelegate> KeyUpDelegateStore;
	TInputDelegateStore<FAnalogInputDelegate> AnalogDelegateStore;
	TInputDelegateStore<FPointerInputDelegate> MouseMoveDelegateStore;
	TInputDelegateStore<FPointerInputDelegate> MouseButtonDownDelegateStore;
	TInputDelegateStore<FPointerInputDelegate> MouseButtonUpDelegateStore;
	TInputDelegateStore<FPointerInputDelegate> MouseButtonDoubleClickDelegateStore;
	TInputDelegateStore<FPointerInputDelegate> MouseWheelDelegateStore;
	TInputDelegateStore<FMotionInputEvent> MotionDelegateStore;

	bool bShouldConsumeGamepadInput = false;
};