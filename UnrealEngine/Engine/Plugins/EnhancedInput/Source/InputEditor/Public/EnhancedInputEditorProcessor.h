// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

struct FInputKeyParams;

/**
 * The Enhanced Input Editor Processor is used to pass InputKey events to the
 * Enhanced Input Editor Subsystem.
 *
 * It will not steal input, and all the functions here will return "False"
 * so that other Input Processors still run with all their normal considerations. 
 */
class FEnhancedInputEditorProcessor : public IInputProcessor
{
public:
	//~ Begin IInputProcessor interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;
	virtual const TCHAR* GetDebugName() const { return TEXT("EnhancedInputEditorPreprocessor"); }
	//~ End IInputProcessor interface

protected:

	void UpdateCachedPointerPosition(const FPointerEvent& MouseEvent);

	/** Called on Tick to process any accumulated mouse input */
	void ProcessAccumulatedPointerInput(float DeltaTime);
	
	/** Tells the Enhanced Input Editor subsystem about the given key event */
	bool InputKeyToSubsystem(const FInputKeyParams& Params);

	/**
	 * The number of cursor samples that happened this frame. The X and Y will be incremented when there is a mouse move event this frame
	 * and it will be reset on ProcessAccumulatedPointerInput.
	 */
	FIntPoint NumCursorSamplesThisFrame = FIntPoint::ZeroValue;
	
	/** The value of the mouse cursor from the most recent mouse event */
	FVector2D CachedCursorDelta = FVector2D::ZeroVector;
};