// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Containers/Ticker.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateTypes.h"
#endif

/**
 * Helper class that is designed to fire before any UI has a chance to process input so that
 * we can properly set the current input type of the application.
 */
class COMMONINPUT_API FCommonInputPreprocessor : public IInputProcessor
{
public:
	FCommonInputPreprocessor(UCommonInputSubsystem& InCommonInputSubsystem);

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("CommonInput"); }
	//~ End IInputProcessor Interface

	void SetInputTypeFilter(ECommonInputType InputType, FName InReason, bool InFilter);

	bool IsInputMethodBlocked(ECommonInputType InputType) const;

	FGamepadChangeDetectedEvent OnGamepadChangeDetected;

protected:
	bool IsRelevantInput(FSlateApplication& SlateApp, const FInputEvent& InputEvent, const ECommonInputType DesiredInputType);

	void RefreshCurrentInputMethod(ECommonInputType InputMethod);

	ECommonInputType GetInputType(const FKey& Key);

	ECommonInputType GetInputType(const FPointerEvent& PointerEvent);
	
protected:
	UCommonInputSubsystem& InputSubsystem;
	
	bool bIgnoreNextMove = false;
	bool InputMethodPermissions[(uint8)ECommonInputType::Count];

	// The reasons we might be filtering input right now.
	TMap<FName, bool> FilterInputTypeWithReasons[(uint8)ECommonInputType::Count];

	FName LastSeenGamepadInputDeviceName;
	FString LastSeenGamepadHardwareDeviceIdentifier;

	friend class UCommonInputSubsystem;
};