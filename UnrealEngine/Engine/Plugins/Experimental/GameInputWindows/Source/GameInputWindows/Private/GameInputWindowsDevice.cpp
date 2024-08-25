// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputWindowsDevice.h"

#if GAME_INPUT_SUPPORT

#include "GameInputUtils.h"
#include "GameInputLogging.h"
#include "HAL/IConsoleManager.h" // FAutoConsoleVariableRef
#include "GenericPlatform/IInputInterface.h"
#include "GameInputDeveloperSettings.h"

FGameInputWindowsInputDevice::FGameInputWindowsInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, IGameInput* InGameInput)
	: IGameInputDeviceInterface(InMessageHandler, InGameInput)
{
	
}

FGameInputWindowsInputDevice::~FGameInputWindowsInputDevice()
{
	
}

GameInputKind FGameInputWindowsInputDevice::GetCurrentGameInputKindSupport() const
{
	GameInputKind RegisterInputKindMask = IGameInputDeviceInterface::GetCurrentGameInputKindSupport();

	// For now, we want to explicitly make sure that Keyboard and Mouse are NOT being processed by GameInput on windows.
	// This is because the WindowsApplication is doing to do all the processing of these types for us already, and 
	// having the Game Input device plugin process them too would result in "double" events and mouse accumulation.

#if !UE_BUILD_SHIPPING
	static bool bLogOnce = false;
	if (!bLogOnce)
	{
		if ((RegisterInputKindMask & GameInputKindKeyboard) != 0)
		{
			UE_LOG(LogGameInput, Log, TEXT("[FGameInputWindowsInputDevice::GetCurrentGameInputKindSupport] Keyboard support was requested, but is not currently supported via the GameInput plugin on Windows."));
		}

		if ((RegisterInputKindMask & GameInputKindMouse) != 0)
		{
			UE_LOG(LogGameInput, Log, TEXT("[FGameInputWindowsInputDevice::GetCurrentGameInputKindSupport] Mouse support was requested, but is not currently supported via the GameInput plugin on Windows."));
		}
		bLogOnce = true;
	}
#endif	// !UE_BUILD_SHIPPING

	// Clear the bits on these types to make sure we don't process any of them
	RegisterInputKindMask &= ~GameInputKindKeyboard;
	RegisterInputKindMask &= ~GameInputKindMouse;

	return RegisterInputKindMask;
}

void FGameInputWindowsInputDevice::HandleDeviceDisconnected(IGameInputDevice* Device, uint64 Timestamp)
{	
	if (Device)
	{
		if (FGameInputDeviceContainer* Data = GetDeviceData(Device))
		{
			// Clear any input state that might be related to this device
			Data->ClearInputState(GameInput);

			// Set it's device to nullptr because it is now disconnected
			Data->SetGameInputDevice(nullptr);
			UE_LOG(LogGameInput, Log, TEXT("Game Input Device '%s' Disconnected Successfully at Input Device ID '%d'"), *UE::GameInput::LexToString(Device), Data->GetDeviceId().GetId());

			// Remap this device to the "unpaired" user because it has been disconnected
			const FPlatformUserId NewUserToAssign = IPlatformInputDeviceMapper::Get().GetUserForUnpairedInputDevices();
			const FInputDeviceId DeviceId = Data->GetDeviceId();

			const bool bSuccess = IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(DeviceId, NewUserToAssign, EInputDeviceConnectionState::Disconnected);
			if (bSuccess)
			{
				Data->SetPlatformUserId(NewUserToAssign);
			}
		}
		else
		{
			UE_LOG(LogGameInput, Error, TEXT("Game Input failed to disconnect a device. The Device '%s' did not have an associated FGameInputWindowsInputDevice!"), *UE::GameInput::LexToString(Device));
		}
	}
	else
	{
		UE_LOG(LogGameInput, Warning, TEXT("Game Input failed to disconnect a device. The Device was null! %s"), *UE::GameInput::LexToString(Device));
	}

	EnumerateCurrentlyConnectedDeviceTypes();
}

void FGameInputWindowsInputDevice::HandleDeviceConnected(IGameInputDevice* Device, uint64 Timestamp)
{
	// get device information
	const GameInputDeviceInfo* Info = Device->GetDeviceInfo();
	UE_LOG(LogGameInput, Log, TEXT("Game Input Device Connected: %s  of kind: %s"), *UE::GameInput::LexToString(Device), *UE::GameInput::LexToString(Info->supportedInput));
	
	FGameInputDeviceContainer* Data = GetOrCreateDeviceData(Device);
	check(Data);

	// Map this input device to its user	
	const FInputDeviceId DeviceId = Data->GetDeviceId();

	// TODO: This will map every game input device to the "primary" user. This is NOT what we want in the long term.
	// Long term, we want you to be able to configure how input devices are mapped to local players in UE with some kind of
	// schema or config variable. We should add this as a setting in the Developer Settings object here.
	const FPlatformUserId UserToAssign = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	
	const bool bSuccess = IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(DeviceId, UserToAssign, EInputDeviceConnectionState::Connected);
	if (ensure(bSuccess))
	{
		Data->SetPlatformUserId(UserToAssign);
	}

	UE_LOG(LogGameInput, Log, TEXT("Using PlatformUserId %d and InputDeviceId %d for device %s"), UserToAssign.GetInternalId(), DeviceId.GetId(), *UE::GameInput::LexToString(Device));

	EnumerateCurrentlyConnectedDeviceTypes();
}

FGameInputDeviceContainer* FGameInputWindowsInputDevice::CreateDeviceData(IGameInputDevice* InDevice)
{
	TSharedPtr<FGameInputDeviceContainer> Container = MakeShared<FGameInputDeviceContainer>(MessageHandler, InDevice, GetCurrentGameInputKindSupport());
	Container->InitalizeDeviceProcessors();

	// This is a new device, we need to assign a new input device ID from the platform user
	const FInputDeviceId AssignedInputDeviceId = IPlatformInputDeviceMapper::Get().AllocateNewInputDeviceId();
	Container->SetInputDeviceId(AssignedInputDeviceId);

	DeviceData.Emplace(Container);

	return Container.Get();
}

#endif	//#if GAME_INPUT_SUPPORT