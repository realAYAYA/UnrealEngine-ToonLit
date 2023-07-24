// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"

class FViewport;

struct FInputKeyEventArgs
{
public:
	FInputKeyEventArgs(FViewport* InViewport, int32 InControllerId, FKey InKey, EInputEvent InEvent)
		: Viewport(InViewport)
		, ControllerId(InControllerId)
		, InputDevice(FInputDeviceId::CreateFromInternalId(InControllerId))
		, Key(InKey)
		, Event(InEvent)
		, AmountDepressed(1.0f)
		, bIsTouchEvent(false)
	{
	}

	FInputKeyEventArgs(FViewport* InViewport, FInputDeviceId InInputDevice, FKey InKey, EInputEvent InEvent)
		: Viewport(InViewport)
		, InputDevice(InInputDevice)
		, Key(InKey)
		, Event(InEvent)
		, AmountDepressed(1.0f)
		, bIsTouchEvent(false)
	{
		FPlatformUserId UserID = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(InInputDevice);
		ControllerId = FPlatformMisc::GetUserIndexForPlatformUser(UserID);
	}

	FInputKeyEventArgs(FViewport* InViewport, int32 InControllerId, FKey InKey, EInputEvent InEvent, float InAmountDepressed, bool bInIsTouchEvent)
		: Viewport(InViewport)
		, ControllerId(InControllerId)
		, InputDevice(FInputDeviceId::CreateFromInternalId(InControllerId))
		, Key(InKey)
		, Event(InEvent)
		, AmountDepressed(InAmountDepressed)
		, bIsTouchEvent(bInIsTouchEvent)
	{
	}
	
	FInputKeyEventArgs(FViewport* InViewport, FInputDeviceId InInputDevice, FKey InKey, EInputEvent InEvent, float InAmountDepressed, bool bInIsTouchEvent)
		: Viewport(InViewport)
		, InputDevice(InInputDevice)
		, Key(InKey)
		, Event(InEvent)
		, AmountDepressed(InAmountDepressed)
		, bIsTouchEvent(bInIsTouchEvent)
	{
		FPlatformUserId UserID = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(InInputDevice);
		ControllerId = FPlatformMisc::GetUserIndexForPlatformUser(UserID);
	}

	bool IsGamepad() const { return Key.IsGamepadKey(); }

public:

	// The viewport which the key event is from.
	FViewport* Viewport;
	// The controller which the key event is from.
	int32 ControllerId;
	// The input device which this key event is from
	FInputDeviceId InputDevice;
	// The type of event which occurred.
	FKey Key;
	// The type of event which occurred.
	EInputEvent Event;
	// For analog keys, the depression percent.
	float AmountDepressed;
	// input came from a touch surface.This may be a faked mouse button from touch.
	bool bIsTouchEvent;
};
