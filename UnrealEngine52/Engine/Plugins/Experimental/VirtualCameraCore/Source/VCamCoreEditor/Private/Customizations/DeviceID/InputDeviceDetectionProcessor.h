// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Framework/Application/IInputProcessor.h"
#include "Templates/SharedPointer.h"

namespace UE::VCamCoreEditor::Private
{
	DECLARE_DELEGATE_OneParam(FOnInputDeviceDetected, int32)

	struct FInputDeviceSelectionSettings
	{
		/** Whether analog input can cause detection of an input device. Some input devices trigger an analog input every tick. */
		bool bAllowAnalog = false;
	};
	
	/** Puts itself at top of input stack and blocks any input. Used to easily determine input devices (except for mice). */
	class FInputDeviceDetectionProcessor : public IInputProcessor, public TSharedFromThis<FInputDeviceDetectionProcessor>
	{
		template <typename ObjectType, ESPMode Mode>
		friend class SharedPointerInternals::TIntrusiveReferenceController;
	public:

		/** Registers a new input processor which will fire OnInputDeviceDetectedDelegate until Unregister is called. */
		static TSharedPtr<FInputDeviceDetectionProcessor> MakeAndRegister(FOnInputDeviceDetected OnInputDeviceDetectedDelegate, FInputDeviceSelectionSettings Settings);
		/** Unregisters this input processor so it will no longer call OnInputDeviceDetectedDelegate nor intercept input. */
		void Unregister();

		/** Called when the user changes the settings */
		void UpdateInputSettings(FInputDeviceSelectionSettings Settings);
		
		//~ Begin IInputProcessor interface
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
		
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
		
		// Detection for mice is undesirable
		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent); }
		virtual bool HandleMouseButtonDownEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return IInputProcessor::HandleMouseButtonDownEvent(SlateApp, MouseEvent); }
		virtual bool HandleMouseButtonUpEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return IInputProcessor::HandleMouseButtonUpEvent(SlateApp, MouseEvent); }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return IInputProcessor::HandleMouseButtonDoubleClickEvent(SlateApp, MouseEvent); }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return IInputProcessor::HandleMouseWheelOrGestureEvent(SlateApp, InWheelEvent, InGestureEvent); }
		
		virtual const TCHAR* GetDebugName() const { return TEXT("FInputDeviceDetectionProcessor"); }
		//~ End IInputProcessor interface

	private:
		
		FInputDeviceDetectionProcessor(FOnInputDeviceDetected OnInputDeviceDetectedDelegate, FInputDeviceSelectionSettings Settings);

		/** Called when any input other than mouse input is received */
		FOnInputDeviceDetected OnInputDeviceDetectedDelegate;

		/** Usability settings exposed by input capturing button */
		FInputDeviceSelectionSettings Settings;
	};
}


