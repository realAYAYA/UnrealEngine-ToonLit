// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Math/Color.h"

/**
 * Interface class for input devices.
 */
class FHoloLensInputInterface
	: public IInputInterface
{
public:

	static TSharedRef< FHoloLensInputInterface > Create( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

public:

	~FHoloLensInputInterface();

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime );

	/** Poll for controller state and send events if needed. */
	void SendControllerEvents();

	/** Sets the strength/speed of the given channel for the given controller id. */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;

	/** Sets the strength/speed of all the channels for the given controller id. */
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override;

	/* Ignored if controller does not support a color. */
	virtual void SetLightColor(int32 ControllerId, struct FColor Color) override { /* nop */ }

	virtual void ResetLightColor(int32 ControllerId) override {}


public:

	/** Platform-specific code can call this function to get the Gamepad^ associated with a UserId, if any. */
	//Windows::Gaming::Input::Gamepad^ GetGamepadForUser(const int UserId);

	/** Platform-specific code can call this function to get the UserId associated with a Gamepad^, if any. */
	int GetUserIdForController(Windows::Gaming::Input::Gamepad^ Controller);

	/** Platform-specific backdoor to get the FPlatformUserId from an XboxUserId */
	//FPlatformUserId GetPlatformUserIdFromXboxUserId(const TCHAR* XboxUserId);

	/** Platform-specific code can call this function to get the Controller^ associated with a ControllerIndex, if any. */
	Windows::Gaming::Input::Gamepad^ GetGamepadForControllerId(int32 ControllerId);

private:

	FHoloLensInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler );

	/**
	 * Called when the application has been reactivated.
	 */
	void OnFocusGain();

	/**
	 * This is called when the application is about to be deactivated.
	 */
	void OnFocusLost();

	/**
	 *	Scans for keyboard input
	 */
	void ConditionalScanForKeyboardChanges( float DeltaTime );

	/**
	 *	Convert the given keycode to the character it represents
	 */
	uint32 MapVirtualKeyToCharacter(uint32 InVirtualKey, bool bShiftIsDown);


private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;

	TSharedRef<class WindowsGamingInputInterface> GamingInput;
};