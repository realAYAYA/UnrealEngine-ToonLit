// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/PlatformInputDeviceMapperLibrary.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlatformInputDeviceMapperLibrary)

int32 UPlatformInputDeviceMapperLibrary::GetAllInputDevicesForUser(const FPlatformUserId UserId, TArray<FInputDeviceId>& OutInputDevices)
{
	return IPlatformInputDeviceMapper::Get().GetAllInputDevicesForUser(UserId, OutInputDevices);
}

int32 UPlatformInputDeviceMapperLibrary::GetAllInputDevices(TArray<FInputDeviceId>& OutInputDevices)
{
	return IPlatformInputDeviceMapper::Get().GetAllInputDevices(OutInputDevices);
}

int32 UPlatformInputDeviceMapperLibrary::GetAllConnectedInputDevices(TArray<FInputDeviceId>& OutInputDevices)
{
	return IPlatformInputDeviceMapper::Get().GetAllConnectedInputDevices(OutInputDevices);
}

int32 UPlatformInputDeviceMapperLibrary::GetAllActiveUsers(TArray<FPlatformUserId>& OutUsers)
{
	return IPlatformInputDeviceMapper::Get().GetAllActiveUsers(OutUsers);
}

FPlatformUserId UPlatformInputDeviceMapperLibrary::GetUserForUnpairedInputDevices()
{
	return IPlatformInputDeviceMapper::Get().GetUserForUnpairedInputDevices();
}

bool UPlatformInputDeviceMapperLibrary::IsUnpairedUserId(const FPlatformUserId PlatformId)
{
	return IPlatformInputDeviceMapper::Get().IsUnpairedUserId(PlatformId);
}

bool UPlatformInputDeviceMapperLibrary::IsInputDeviceMappedToUnpairedUser(const FInputDeviceId InputDevice)
{
	return IPlatformInputDeviceMapper::Get().IsInputDeviceMappedToUnpairedUser(InputDevice);
}

FInputDeviceId UPlatformInputDeviceMapperLibrary::GetDefaultInputDevice()
{
	return IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
}

FPlatformUserId UPlatformInputDeviceMapperLibrary::GetUserForInputDevice(FInputDeviceId DeviceId)
{
	return IPlatformInputDeviceMapper::Get().GetUserForInputDevice(DeviceId);
}

FInputDeviceId UPlatformInputDeviceMapperLibrary::GetPrimaryInputDeviceForUser(FPlatformUserId UserId)
{
	return IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(UserId);
}

EInputDeviceConnectionState UPlatformInputDeviceMapperLibrary::GetInputDeviceConnectionState(const FInputDeviceId DeviceId)
{
	return IPlatformInputDeviceMapper::Get().GetInputDeviceConnectionState(DeviceId);
}

bool UPlatformInputDeviceMapperLibrary::IsValidInputDevice(FInputDeviceId DeviceId)
{
	return DeviceId.IsValid();
}

bool UPlatformInputDeviceMapperLibrary::IsValidPlatformId(FPlatformUserId UserId)
{
	return UserId.IsValid();
}

FPlatformUserId UPlatformInputDeviceMapperLibrary::PlatformUserId_None()
{
	return PLATFORMUSERID_NONE;
}

FInputDeviceId UPlatformInputDeviceMapperLibrary::InputDeviceId_None()
{
	return INPUTDEVICEID_NONE;
}

bool UPlatformInputDeviceMapperLibrary::EqualEqual_PlatformUserId(FPlatformUserId A, FPlatformUserId B)
{
	return A == B;
}

bool UPlatformInputDeviceMapperLibrary::NotEqual_PlatformUserId(FPlatformUserId A, FPlatformUserId B)
{
	return A != B;
}

bool UPlatformInputDeviceMapperLibrary::EqualEqual_InputDeviceId(FInputDeviceId A, FInputDeviceId B)
{
	return A == B;
}

bool UPlatformInputDeviceMapperLibrary::NotEqual_InputDeviceId(FInputDeviceId A, FInputDeviceId B)
{
	return A != B;
}
