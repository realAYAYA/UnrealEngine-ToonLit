// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsGamingInputInterface.h"
#include "HAL/PlatformTime.h"
#include <unordered_map>

DECLARE_LOG_CATEGORY_EXTERN(GamepadSystem, Log, All);
DEFINE_LOG_CATEGORY(GamepadSystem);

using namespace Windows::Foundation;
using namespace Windows::Gaming::Input;

// Button flags assigned by Windows::Gaming::Input
enum { GamingInputButtonMask = 0x3FFF };

// Allocate flags for emulated/extended digital button functionality
enum { GamepadButtonAux_LeftTrigger = (1 << 22) };
enum { GamepadButtonAux_RightTrigger = (1 << 23) };
enum { GamepadButtonAux_LeftStickUp = (1 << 24) };
enum { GamepadButtonAux_LeftStickDown = (1 << 25) };
enum { GamepadButtonAux_LeftStickLeft = (1 << 26) };
enum { GamepadButtonAux_LeftStickRight = (1 << 27) };
enum { GamepadButtonAux_RightStickUp = (1 << 28) };
enum { GamepadButtonAux_RightStickDown = (1 << 29) };
enum { GamepadButtonAux_RightStickLeft = (1 << 30) };
enum { GamepadButtonAux_RightStickRight = (1 << 31) };

// Values taken from XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, XINPUT_GAMEPAD_TRIGGER_THRESHOLD used by XInput.
constexpr float GamepadLeftStickDeadzone = (7849.0f / 32768.0f);
constexpr float GamepadRightStickDeadzone = (8689.0f / 32768.0f);
constexpr float GamepadTriggerDeadzone = (30.0f / 255.0f);


TSharedRef< WindowsGamingInputInterface > WindowsGamingInputInterface::Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	return MakeShareable(new WindowsGamingInputInterface(InMessageHandler));
}

WindowsGamingInputInterface::WindowsGamingInputInterface(const TSharedRef< FGenericApplicationMessageHandler > & InMessageHandler)
	: MessageHandler(InMessageHandler)
	, InitialRepeatDelay(0.2f)
	, SubsequentRepeatDelay(0.1f)
	, UpdateEnabled(true)
	, UpdateChanged(false)
{
	PreviousTime = FPlatformTime::Seconds();

	// Listen to Gamepad Added events
	EventHandler<Gamepad^>^ GamepadAddedEvent = ref new EventHandler<Gamepad^>(
		[this](Platform::Object^, Gamepad^ Gamepad)
	{
		OnGamepadAdded(Gamepad);
	});

	// Listen to Gamepad Removed events
	EventHandler<Gamepad^>^ GamepadRemovedEvent = ref new EventHandler<Gamepad^>(
		[this](Platform::Object^, Gamepad^ Gamepad)
	{
		OnGamepadRemoved(Gamepad);
	});

	GamepadAddedEventToken = Gamepad::GamepadAdded += GamepadAddedEvent;
	GamepadRemovedEventToken = Gamepad::GamepadRemoved += GamepadRemovedEvent;
}

WindowsGamingInputInterface::~WindowsGamingInputInterface()
{
	Gamepad::GamepadAdded -= GamepadAddedEventToken;
	Gamepad::GamepadRemoved -= GamepadRemovedEventToken;
}

void WindowsGamingInputInterface::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler > & InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

void WindowsGamingInputInterface::OnGamepadAdded(Windows::Gaming::Input::Gamepad^ Gamepad)
{
	int Id = InvalidGamepadDeviceId;

	// find slot and perform validation
	for (int i = 0; i < MaxSupportedGamepads; i++)
	{
		if (InvalidGamepadDeviceId != PadInfo[i].DeviceId)
		{
			if (Gamepad == PadInfo[i].Gamepad)
			{
				return;		// already registered
			}
		}
		else if (InvalidGamepadDeviceId == Id)
		{
			Id = i;
		}
	}

	// add new gamepad
	if (InvalidGamepadDeviceId != Id)
	{
		UE_LOG(GamepadSystem, Log, TEXT("Gamepad 0x%p (id = %d) added."), (void *)Gamepad, Id);
		PadInfo[Id].DeviceId = Id;
		PadInfo[Id].Gamepad = Gamepad;
	}
	else
	{
		UE_LOG(GamepadSystem, Log, TEXT("Failed to add Gamepad 0x%p.  (No Id available)"), (void *)Gamepad);
	}
}

void WindowsGamingInputInterface::OnGamepadRemoved(Windows::Gaming::Input::Gamepad^ Gamepad)
{
	// find slot and remove
	for (int i = 0; i < MaxSupportedGamepads; i++)
	{
		if (InvalidGamepadDeviceId != PadInfo[i].DeviceId)
		{
			if (Gamepad == PadInfo[i].Gamepad)
			{
				UE_LOG(GamepadSystem, Log, TEXT("Gamepad 0x%p (id = %d) removed."), (void *)Gamepad, PadInfo[i].DeviceId);
				PadInfo[i].DeviceId = InvalidGamepadDeviceId;
				PadInfo[i].Gamepad = nullptr;
				PadInfo[i].TerminateInputs = true;
				return;
			}
		}
	}

	UE_LOG(GamepadSystem, Log, TEXT("Failed to remove Gamepad 0x%p.  (not found)"), (void *)Gamepad);
}

Windows::Gaming::Input::Gamepad^ WindowsGamingInputInterface::GetGamepadForIndex(int Id)
{
	return ((((unsigned int)Id) < MaxSupportedGamepads) ? PadInfo[Id].Gamepad : nullptr);
}

bool WindowsGamingInputInterface::GetVibration(int Id, float& LeftMotor, float& RightMotor, float& LeftTrigger, float& RightTrigger)
{
	Gamepad^ GamePad = GetGamepadForIndex(Id);
	if (GamePad == nullptr)
	{
		return false;
	}

	LeftMotor = PadInfo[Id].Gamepad->Vibration.LeftMotor;
	RightMotor = PadInfo[Id].Gamepad->Vibration.RightMotor;
	LeftTrigger = PadInfo[Id].Gamepad->Vibration.LeftTrigger;
	RightTrigger = PadInfo[Id].Gamepad->Vibration.RightTrigger;

	return true;
}

bool WindowsGamingInputInterface::SetVibration(int Id, float LeftMotor, float RightMotor, float LeftTrigger, float RightTrigger)
{
	Gamepad^ GamePad = GetGamepadForIndex(Id);
	if (GamePad == nullptr)
	{
		return false;
	}

	if (PadInfo[Id].Gamepad->Vibration.LeftMotor != LeftMotor ||
		PadInfo[Id].Gamepad->Vibration.RightMotor != RightMotor ||
		PadInfo[Id].Gamepad->Vibration.LeftTrigger != LeftTrigger ||
		PadInfo[Id].Gamepad->Vibration.RightTrigger != RightTrigger)
	{
		Windows::Gaming::Input::GamepadVibration Vibration;
		Vibration.LeftMotor = FMath::Clamp(LeftMotor, 0.f, 1.f);
		Vibration.RightMotor = FMath::Clamp(RightMotor, 0.f, 1.f);
		Vibration.LeftTrigger = FMath::Clamp(LeftTrigger, 0.f, 1.f);
		Vibration.RightTrigger = FMath::Clamp(RightTrigger, 0.f, 1.f);

		UE_LOG(GamepadSystem, Verbose, TEXT("Set Gamepad 0x%p (id = %d) vibration. (%f, %f, %f, %f)"), (void *)(PadInfo[Id].Gamepad), Id, Vibration.LeftMotor, Vibration.RightMotor, Vibration.LeftTrigger, Vibration.RightTrigger);
		PadInfo[Id].Gamepad->Vibration = Vibration;
	}

	return true;
}

static bool GamepadButtonToUnrealName(FGamepadKeyNames::Type& ButtonKey, uint32 ButtonMask)
{
	typedef std::unordered_map< uint32, FGamepadKeyNames::Type > ButtonMapType;
	static const ButtonMapType ButtonMap
	{
		{ (uint32)Windows::Gaming::Input::GamepadButtons::A,				FGamepadKeyNames::FaceButtonBottom },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::B,				FGamepadKeyNames::FaceButtonRight },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::X,				FGamepadKeyNames::FaceButtonLeft },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::Y,				FGamepadKeyNames::FaceButtonTop },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::LeftShoulder,		FGamepadKeyNames::LeftShoulder },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::RightShoulder,	FGamepadKeyNames::RightShoulder },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::Menu,				FGamepadKeyNames::SpecialRight },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::View,				FGamepadKeyNames::SpecialLeft },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::DPadUp,			FGamepadKeyNames::DPadUp },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::DPadDown,			FGamepadKeyNames::DPadDown },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::DPadLeft,			FGamepadKeyNames::DPadLeft },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::DPadRight,		FGamepadKeyNames::DPadRight },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::LeftThumbstick,	FGamepadKeyNames::LeftThumb },
		{ (uint32)Windows::Gaming::Input::GamepadButtons::RightThumbstick,	FGamepadKeyNames::RightThumb },

		{ GamepadButtonAux_LeftTrigger,			FGamepadKeyNames::LeftTriggerThreshold },
		{ GamepadButtonAux_RightTrigger,		FGamepadKeyNames::RightTriggerThreshold },
		{ GamepadButtonAux_LeftStickUp,			FGamepadKeyNames::LeftStickUp },
		{ GamepadButtonAux_LeftStickDown,		FGamepadKeyNames::LeftStickDown },
		{ GamepadButtonAux_LeftStickLeft,		FGamepadKeyNames::LeftStickLeft },
		{ GamepadButtonAux_LeftStickRight,		FGamepadKeyNames::LeftStickRight },
		{ GamepadButtonAux_RightStickUp,		FGamepadKeyNames::RightStickUp },
		{ GamepadButtonAux_RightStickDown,		FGamepadKeyNames::RightStickDown },
		{ GamepadButtonAux_RightStickLeft,		FGamepadKeyNames::RightStickLeft },
		{ GamepadButtonAux_RightStickRight,		FGamepadKeyNames::RightStickRight }
	};

	ButtonMapType::const_iterator it = ButtonMap.find(ButtonMask);
	if (ButtonMap.end() != it)
	{
		ButtonKey = it->second;
		return true;
	}

	return false;
}

void WindowsGamingInputInterface::EnableUpdate()
{
	if (!UpdateEnabled)
	{
		UpdateEnabled = true;
		UpdateChanged = true;
	}
}

void WindowsGamingInputInterface::DisableUpdate()
{
	if (UpdateEnabled)
	{
		UpdateEnabled = false;
		UpdateChanged = true;
	}
}

void WindowsGamingInputInterface::TerminateGamepadInputs(GamepadMapping& Mapping, int Id)
{
	UE_LOG(GamepadSystem, Log, TEXT("Terminate active gamepad inputs."));

	// reset axes
	MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, Id, 0.0f);
	MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, Id, 0.0f);
	MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, Id, 0.0f);
	MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, Id, 0.0f);

	if (Mapping.LeftTrigger > 0)
	{
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, Id, 0.0f);
		Mapping.LeftTrigger = 0.0f;
	}
	if (Mapping.RightTrigger > 0)
	{
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, Id, 0.0f);
		Mapping.RightTrigger = 0.0f;
	}

	unsigned int ButtonMask = Mapping.ButtonHeldMask;
	unsigned int BitMask = 1;
	while ((0 != ButtonMask) && (0 != BitMask))
	{
		FName ButtonKey;

		if (0 != (ButtonMask & BitMask))
		{
			if (GamepadButtonToUnrealName(ButtonKey, BitMask))
			{
				MessageHandler->OnControllerButtonReleased(ButtonKey, Id, false);
			}
		}
		BitMask <<= 1;
	}

	Mapping.ButtonHeldMask = 0;
	Mapping.TerminateInputs = false;

	for (uint32 i = 0; i < MaxSupportedButtons; i++)
	{
		Mapping.RepeatTime[i] = 0.0f;
	}
}

void WindowsGamingInputInterface::UpdateGamepads()
{
	const double CurrentTime = FPlatformTime::Seconds();
	const float  DeltaTime = (float)(CurrentTime - PreviousTime);
	PreviousTime = CurrentTime;

	// handle update toggle
	if (UpdateChanged)
	{
		if (UpdateEnabled)
		{
			// terminate existing inputs (e.g.  generate button up events for pressed buttons)
			for (int i = 0; i < MaxSupportedGamepads; i++)
			{
				if ((PadInfo[i].TerminateInputs) ||
					((InvalidGamepadDeviceId != PadInfo[i].DeviceId) &&
						(nullptr != PadInfo[i].Gamepad)))
				{
					int	Id = i;
					TerminateGamepadInputs(PadInfo[i], Id);
				}
			}
		}
		UpdateChanged = false;
	}

	if (UpdateEnabled)
	{
		// find slot and remove
		for (int i = 0; i < MaxSupportedGamepads; i++)
		{
			if ((InvalidGamepadDeviceId != PadInfo[i].DeviceId) &&
				(nullptr != PadInfo[i].Gamepad))
			{
				GamepadReading Reading = PadInfo[i].Gamepad->GetCurrentReading();
				float LeftThumbX = (float)Reading.LeftThumbstickX;
				float LeftThumbY = (float)Reading.LeftThumbstickY;
				float RightThumbX = (float)Reading.RightThumbstickX;
				float RightThumbY = (float)Reading.RightThumbstickY;
				float LeftTrigger = (float)Reading.LeftTrigger;
				float RightTrigger = (float)Reading.RightTrigger;

				// map axes
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, i, LeftThumbX);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, i, LeftThumbY);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, i, RightThumbX);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, i, RightThumbY);

				// map analog triggers directly
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, i, LeftTrigger);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, i, RightTrigger);

				// map buttons (low 15 bits)
				unsigned int CurrentButtonHeldMask = (((unsigned int)Reading.Buttons) & GamingInputButtonMask);

				// map analog triggers to digital input.
				float PrevLeftTrigger = PadInfo[i].LeftTrigger;
				if (LeftTrigger > GamepadTriggerDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_LeftTrigger;
				}
				PadInfo[i].LeftTrigger = LeftTrigger;

				float PrevRightTrigger = PadInfo[i].RightTrigger;
				if (RightTrigger > GamepadTriggerDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_RightTrigger;
				}
				PadInfo[i].RightTrigger = RightTrigger;

				// map left/right stick digital inputs to (top 8 bits)

				// kept h/v key switch due to following change.
				// Only send a single direction of left stick analog readings as a key down event at a time.
				// - This is to work around the fact that SWidget navigation for the left stick responds to OnKeyDown rather than OnAnalogValue
				// - To get around that without this change would require a number of changes to the way SWidget handles navigation, 
				// - and not only that, but any of our UMG screens that implement OnPreviewKeyDown or OnKeyDown themselves to handle left stick navigation manually would need changes as well.
				// - UDN post: https://udn.unrealengine.com/questions/278514/overly-sensitive-umg-navigation.html
				const bool UseLeftstickVerticalReading = FMath::Abs(LeftThumbY) >= FMath::Abs(LeftThumbX);
				if (UseLeftstickVerticalReading)
				{
					if (LeftThumbY > GamepadLeftStickDeadzone)
					{
						CurrentButtonHeldMask |= GamepadButtonAux_LeftStickUp;
					}
					else if (LeftThumbY < -GamepadLeftStickDeadzone)
					{
						CurrentButtonHeldMask |= GamepadButtonAux_LeftStickDown;
					}
				}
				else
				{
					if (LeftThumbX > GamepadLeftStickDeadzone)
					{
						CurrentButtonHeldMask |= GamepadButtonAux_LeftStickRight;
					}
					else if (LeftThumbX < -GamepadLeftStickDeadzone)
					{
						CurrentButtonHeldMask |= GamepadButtonAux_LeftStickLeft;
					}
				}

				if (RightThumbY > GamepadRightStickDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_RightStickUp;
				}
				else if (RightThumbY < -GamepadRightStickDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_RightStickDown;
				}
				if (RightThumbX > GamepadRightStickDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_RightStickRight;
				}
				else if (RightThumbX < -GamepadRightStickDeadzone)
				{
					CurrentButtonHeldMask |= GamepadButtonAux_RightStickLeft;
				}

				// handle button change events
				unsigned int ActionMask = (PadInfo[i].ButtonHeldMask ^ CurrentButtonHeldMask);
				unsigned int RepeatMask = (PadInfo[i].ButtonHeldMask & CurrentButtonHeldMask);
				unsigned int BitMask = 1;

				for (uint32 n = 0; n < MaxSupportedButtons; ++n)
				{
					FGamepadKeyNames::Type ButtonKey;

					// check for button state change
					if (0 != (ActionMask & BitMask))
					{
						if (0 != (CurrentButtonHeldMask & BitMask))
						{
							if (GamepadButtonToUnrealName(ButtonKey, BitMask))
							{
								UE_LOG(GamepadSystem, Verbose, TEXT("Gamepad 0x%p (id = %d) - %s Pressed"), (void *)(PadInfo[i].Gamepad), i, *ButtonKey.ToString());
								MessageHandler->OnControllerButtonPressed(ButtonKey, i, false);
								PadInfo[i].RepeatTime[n] = InitialRepeatDelay;
							}
						}
						else
						{
							if (GamepadButtonToUnrealName(ButtonKey, BitMask))
							{
								UE_LOG(GamepadSystem, Verbose, TEXT("Gamepad 0x%p (id = %d) - %s Released"), (void *)(PadInfo[i].Gamepad), i, *ButtonKey.ToString());
								MessageHandler->OnControllerButtonReleased(ButtonKey, i, false);
								PadInfo[i].RepeatTime[n] = 0.0f;
							}
						}
					}

					// check for repeat key
					if (0 != (RepeatMask & BitMask))
					{
						PadInfo[i].RepeatTime[n] -= DeltaTime;
						if (PadInfo[i].RepeatTime[n] <= 0.0f)
						{
							PadInfo[i].RepeatTime[n] = SubsequentRepeatDelay;
							if (GamepadButtonToUnrealName(ButtonKey, BitMask))
							{
								UE_LOG(GamepadSystem, Verbose, TEXT("Gamepad 0x%p (id = %d) - %s Repeated"), (void *)(PadInfo[i].Gamepad), i, *ButtonKey.ToString());
								MessageHandler->OnControllerButtonPressed(ButtonKey, i, true);
							}
						}
					}

					BitMask <<= 1;
				}

				// update button states
				PadInfo[i].ButtonHeldMask = CurrentButtonHeldMask;
			}
		}
	}
}
