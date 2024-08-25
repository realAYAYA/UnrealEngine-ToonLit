// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EOSShared.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformInput.h"

#include "eos_ui_types.h"

struct FEOSInputState : EOS_UI_ReportInputStateOptions
{
	FEOSInputState()
	{
		ApiVersion = 2;
		UE_EOS_CHECK_API_MISMATCH(EOS_UI_REPORTINPUTSTATE_API_LATEST, 2);
		bAcceptIsFaceButtonRight = FPlatformInput::GetGamepadAcceptKey() == EKeys::Gamepad_FaceButton_Right;
		ButtonDownFlags = EOS_UI_EInputStateButtonFlags::EOS_UISBF_None;
		bMouseButtonDown = EOS_FALSE;
		MousePosX = 0;
		MousePosY = 0;
		GamepadIndex = 0;
		LeftStickX = 0.0f;
		LeftStickY = 0.0f;
		RightStickX = 0.0f;
		RightStickY = 0.0f;
		LeftTrigger = 0.0f;
		RightTrigger = 0.0f;
	}

	FEOSInputState& WithButtonDownFlags(const EOS_UI_EInputStateButtonFlags InButtonDownFlags)
	{
		ButtonDownFlags = InButtonDownFlags;
		return *this;
	}

	FEOSInputState& WithMouseButtonDown(const bool InMouseButtonDown)
	{
		bMouseButtonDown = InMouseButtonDown;
		return *this;
	}

	FEOSInputState& WithMousePosX(const uint32_t InMousePosX)
	{
		MousePosX = InMousePosX;
		return *this;
	}

	FEOSInputState& WithMousePosY(const uint32_t InMousePosY)
	{
		MousePosY = InMousePosY;
		return *this;
	}

	FEOSInputState& WithGamepadIndex(const uint32_t InGamepadIndex)
	{
		GamepadIndex = InGamepadIndex;
		return *this;
	}

	FEOSInputState& WithLeftStick(const float X, const float Y)
	{
		LeftStickX = X;
		LeftStickY = Y;
		return *this;
	}

	FEOSInputState& WithRightStick(const float X, const float Y)
	{
		RightStickX = X;
		RightStickY = Y;
		return *this;
	}

	FEOSInputState& WithLeftTrigger(const float InLeftTrigger)
	{
		LeftTrigger = InLeftTrigger;
		return *this;
	}

	FEOSInputState& WithRightTrigger(const float InRightTrigger)
	{
		RightTrigger = InRightTrigger;
		return *this;
	}
};

class FEOSOverlayInputProviderPreProcessor : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override;

private:
	virtual void HandleInput(const FEOSInputState& NewInputState);
	bool ShouldConsumeInput(FSlateApplication& SlateApp);
	bool ProcessInputEvent(FSlateApplication& SlateApp, const FEOSInputState& NewInputState);
	FEOSInputState& GetCurrentInputState(uint32_t GamepadIndex);
	const TMap<FKey, EOS_UI_EInputStateButtonFlags>& GetUEKeyToEOSKeyMap();

private:
	/** False if we have received an EOS_NotImplemented from EOS_UI_ReportInputState */
	bool bIsReportInputStateSupported = true;

	bool bIsRenderReady = false;

	TMap<uint32_t, FEOSInputState> CurrentInputStates;
};