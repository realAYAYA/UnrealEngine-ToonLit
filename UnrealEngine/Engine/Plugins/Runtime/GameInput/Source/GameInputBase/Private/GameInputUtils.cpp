// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputUtils.h"

#if GAME_INPUT_SUPPORT

namespace UE::GameInput
{
	FString LexToString(IGameInputDevice* Device)
	{
		FString Result = TEXT("<invalid device>");

		if (Device)
		{
			const GameInputDeviceInfo* Info = Device->GetDeviceInfo();
			const bool bIsVirtualDevice = Info->deviceFamily == GameInputFamilyVirtual;
			
			Result = FString::Printf(TEXT("%llp   DeviceName: %s %s ProdId: %04x (%u) VendId: %04x (%u)"), 
				Device, 
				Info->displayName, 
				bIsVirtualDevice ? TEXT("(virtual)") : TEXT(""),
				Info->productId, (uint32)Info->productId,
				Info->vendorId, (uint32)Info->vendorId);
		}

		return Result;
	}
	
	FString LexToString(GameInputDeviceStatus DeviceStatus)
	{
		if (DeviceStatus == GameInputDeviceNoStatus)
		{
			return TEXT("NoStatus");
		}
		else if (DeviceStatus == GameInputDeviceAnyStatus)
		{
			return TEXT("Any");
		}

		FString Result = TEXT("");
#define DEVICE_STATUS(StatusFlag,DisplayName) if( DeviceStatus & StatusFlag ) Result += (FString(DisplayName) + TEXT("|"));
		DEVICE_STATUS(GameInputDeviceConnected, TEXT("Connected"));
		DEVICE_STATUS(GameInputDeviceInputEnabled, TEXT("InputEnabled"));
		DEVICE_STATUS(GameInputDeviceOutputEnabled, TEXT("OutputEnabled"));
		DEVICE_STATUS(GameInputDeviceRawIoEnabled, TEXT("RawIoEnabled"));
		DEVICE_STATUS(GameInputDeviceAudioCapture, TEXT("AudioCapture"));
		DEVICE_STATUS(GameInputDeviceAudioRender, TEXT("AudioRender"));
		DEVICE_STATUS(GameInputDeviceSynchronized, TEXT("Synchronized"));
		DEVICE_STATUS(GameInputDeviceWireless, TEXT("Wireless"));
		DEVICE_STATUS(GameInputDeviceUserIdle, TEXT("UserIdle"));
#undef DEVICE_STATUS

		Result.RemoveFromEnd(TEXT("|"));
		return Result;
	}
	
	FString LexToString(GameInputKind InputKind)
	{
		if (InputKind == GameInputKindUnknown)
		{
			return TEXT("Unknown");
		}

		FString Result = TEXT("");
#define INPUT_KIND_STRING(StatusFlag, DisplayName) if( InputKind & StatusFlag ) Result += (FString(DisplayName) + TEXT("|"));
		INPUT_KIND_STRING(GameInputKindRawDeviceReport, TEXT("RawDeviceReport"));
		INPUT_KIND_STRING(GameInputKindControllerAxis, TEXT("ControllerAxis"));
		INPUT_KIND_STRING(GameInputKindControllerButton, TEXT("ControllerButton"));
		INPUT_KIND_STRING(GameInputKindControllerSwitch, TEXT("ControllerSwitch"));
		INPUT_KIND_STRING(GameInputKindController, TEXT("Controller"));
		INPUT_KIND_STRING(GameInputKindKeyboard, TEXT("Keyboard"));
		INPUT_KIND_STRING(GameInputKindMouse, TEXT("Mouse"));
		INPUT_KIND_STRING(GameInputKindTouch, TEXT("Touch"));
		INPUT_KIND_STRING(GameInputKindMotion, TEXT("Motion"));
		INPUT_KIND_STRING(GameInputKindArcadeStick, TEXT("ArcadeStick"));
		INPUT_KIND_STRING(GameInputKindFlightStick, TEXT("FlightStick"));
		INPUT_KIND_STRING(GameInputKindGamepad, TEXT("Gamepad"));
		INPUT_KIND_STRING(GameInputKindRacingWheel, TEXT("RacingWheel"));
		INPUT_KIND_STRING(GameInputKindUiNavigation, TEXT("UiNavigation"));
#undef INPUT_KIND_STRING

		Result.RemoveFromEnd(TEXT("|"));
		return Result;
	}

	FString LexToString(GameInputSwitchPosition SwitchPos)
	{
		switch(SwitchPos)
		{
		case GameInputSwitchCenter: return TEXT("SwitchCenter");
		case GameInputSwitchUp: return TEXT("SwitchUp");
		case GameInputSwitchUpRight: return TEXT("SwitchUpRight");
		case GameInputSwitchRight: return TEXT("SwitchRight");
		case GameInputSwitchDownRight: return TEXT("SwitchDownRight");
		case GameInputSwitchDown: return TEXT("SwitchDown");
		case GameInputSwitchDownLeft: return TEXT("SwitchDownLeft");
		case GameInputSwitchLeft: return TEXT("SwitchLeft");
		case GameInputSwitchUpLeft: return TEXT("SwitchUpLeft");
		}

		return TEXT("Unknown Switch Position");
	}

	FString LexToString(const APP_LOCAL_DEVICE_ID& DeviceId)
	{
		return BytesToHex(DeviceId.value, APP_LOCAL_DEVICE_ID_SIZE);
	}
	
	EInputDeviceConnectionState DeviceStateToConnectionState(GameInputDeviceStatus InCurrentStatus, GameInputDeviceStatus InPreviousStatus)
	{
		InCurrentStatus = InCurrentStatus & GameInputDeviceConnected;
		InPreviousStatus = InPreviousStatus & GameInputDeviceConnected;

		// If the current status has changed to connected, or the previous and current status are connected, then respond as such
		// The current and previous state may both respond as connected upon Application boot
		if ((InCurrentStatus > InPreviousStatus) || (InCurrentStatus && InPreviousStatus))
		{
			return EInputDeviceConnectionState::Connected;
		}
		else if (InPreviousStatus > InCurrentStatus)
		{
			return EInputDeviceConnectionState::Disconnected;
		}
		else
		{
			// Returned in the case of there being no connection change
			return EInputDeviceConnectionState::Invalid;
		}
	}

	const TCHAR* GetMouseButtonName(EMouseButtons::Type MouseButton)
	{
		switch (MouseButton)
		{
		case EMouseButtons::Left: return TEXT("Left");
		case EMouseButtons::Right: return TEXT("Right");
		case EMouseButtons::Middle: return TEXT("Middle");
		case EMouseButtons::Thumb01: return TEXT("Thumb01");
		case EMouseButtons::Thumb02: return TEXT("Thumb02");
		}
		return TEXT("Invalid");
	}

	bool GameInputButtonToUnrealName(const TMap<uint32, FGamepadKeyNames::Type>& UEButtonMap, uint32 ButtonMask, OUT FGamepadKeyNames::Type& OutKeyName)
	{
		if (const FGamepadKeyNames::Type* Name = UEButtonMap.Find(ButtonMask))
		{
			OutKeyName = *Name;
			return true;
		}

		return false;
	}

	const TMap<GameInputSwitchPosition, TArray<FGamepadKeyNames::Type>>& GetSwitchButtonMap()
	{
		static const TMap<GameInputSwitchPosition, TArray<FGamepadKeyNames::Type>> SwitchButtonMap
		{
			{ GameInputSwitchCenter,			{ } },
			{ GameInputSwitchUp,				{ FGamepadKeyNames::DPadUp } },
			{ GameInputSwitchUpRight,			{ FGamepadKeyNames::DPadUp, FGamepadKeyNames::DPadRight } },
			{ GameInputSwitchRight,				{ FGamepadKeyNames::DPadRight } },
			{ GameInputSwitchDownRight,			{ FGamepadKeyNames::DPadDown, FGamepadKeyNames::DPadRight } },
			{ GameInputSwitchDown,				{ FGamepadKeyNames::DPadDown } },
			{ GameInputSwitchDownLeft,			{ FGamepadKeyNames::DPadDown, FGamepadKeyNames::DPadLeft } },
			{ GameInputSwitchLeft,				{ FGamepadKeyNames::DPadLeft } },
			{ GameInputSwitchUpLeft,			{ FGamepadKeyNames::DPadUp, FGamepadKeyNames::DPadLeft } },
		};
		return SwitchButtonMap;
	}

	const TArray<FGamepadKeyNames::Type>* SwitchPositionToUnrealName(const GameInputSwitchPosition ButtonMask)
	{
		return GetSwitchButtonMap().Find(ButtonMask);
	}
}

#endif	// #if GAME_INPUT_SUPPORT