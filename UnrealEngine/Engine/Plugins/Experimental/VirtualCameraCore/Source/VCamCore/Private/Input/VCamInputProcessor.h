// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UInputVCamSubsystem;
struct FInputKeyParams;

namespace UE::VCamCore::Private
{
	/**
	 * Passes inputs to UVCamPlayerInput::InputKey.
	 * 
	 * This code is a modified version of FEnhancedInputEditorProcessor; the only difference is where the input ends up
	 * getting sent to.
	 */
	class FVCamInputProcessor : public IInputProcessor
	{
	public:

		FVCamInputProcessor(UInputVCamSubsystem& OwningSubsystem);
		
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
		virtual const TCHAR* GetDebugName() const { return TEXT("FVCamInputProcessor"); }
		//~ End IInputProcessor interface

	private:

		TWeakObjectPtr<UInputVCamSubsystem> OwningSubsystem;

		/**
		 * The number of cursor samples that happened this frame. The X and Y will be incremented when there is a mouse move event this frame
		 * and it will be reset on ProcessAccumulatedPointerInput.
		 */
		FIntPoint NumCursorSamplesThisFrame = FIntPoint::ZeroValue;
	
		/** The value of the mouse cursor from the most recent mouse event */
		FVector2D CachedCursorDelta = FVector2D::ZeroVector;

		void UpdateCachedPointerPosition(const FPointerEvent& MouseEvent);
		/** Called on Tick to process any accumulated mouse input */
		void ProcessAccumulatedPointerInput(float DeltaTime);
	
		/** Tells the Enhanced Input Editor subsystem about the given key event */
		bool InputKeyToSubsystem(const FInputKeyParams& Params);
	};
}


