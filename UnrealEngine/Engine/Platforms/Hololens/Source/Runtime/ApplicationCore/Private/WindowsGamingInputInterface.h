// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericApplicationMessageHandler.h"

class WindowsGamingInputInterface
{
public:

	static TSharedRef< WindowsGamingInputInterface > Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	~WindowsGamingInputInterface();

	void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler > & InMessageHandler);

	void UpdateGamepads();

	bool GetVibration(int Id, float& LeftMotor, float& RightMotor, float& LeftTrigger, float& RightTrigger);

	bool SetVibration(int Id, float LeftMotor, float RightMotor, float LeftTrigger, float RightTrigger);

	void EnableUpdate();

	void DisableUpdate();

	Windows::Gaming::Input::Gamepad^ GetGamepadForIndex(int Id);

protected:

	void OnGamepadAdded(Windows::Gaming::Input::Gamepad^ Gamepad);
	void OnGamepadRemoved(Windows::Gaming::Input::Gamepad^ Gamepad);

private:

	WindowsGamingInputInterface(const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler);

private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	Windows::Foundation::EventRegistrationToken		GamepadAddedEventToken;
	Windows::Foundation::EventRegistrationToken		GamepadRemovedEventToken;
	double											PreviousTime;
	float											InitialRepeatDelay;
	float											SubsequentRepeatDelay;

	enum { MaxSupportedGamepads = 4 };		// MAX_NUM_XINPUT_CONTROLLER == 4 (in XInput manager)
	enum { InvalidGamepadDeviceId = -1 };
	enum { MaxSupportedButtons = 32 };

	class GamepadMapping
	{
	public:
		int									DeviceId;
		Windows::Gaming::Input::Gamepad^	Gamepad;
		unsigned int						ButtonHeldMask;
		float								LeftTrigger;
		float								RightTrigger;
		bool								TerminateInputs;
		float								RepeatTime[MaxSupportedButtons];

		GamepadMapping()
			: DeviceId(InvalidGamepadDeviceId)
			, ButtonHeldMask(0)
			, LeftTrigger(0.0f)
			, RightTrigger(0.0f)
			, TerminateInputs(false)
		{
			for (uint32 i = 0; i < MaxSupportedButtons; i++)
			{
				RepeatTime[i] = 0.0f;
			}
		}
	};

	GamepadMapping		PadInfo[MaxSupportedGamepads];
	bool				UpdateEnabled;
	bool				UpdateChanged;

	void TerminateGamepadInputs(GamepadMapping& Mapping, int Id);
};

