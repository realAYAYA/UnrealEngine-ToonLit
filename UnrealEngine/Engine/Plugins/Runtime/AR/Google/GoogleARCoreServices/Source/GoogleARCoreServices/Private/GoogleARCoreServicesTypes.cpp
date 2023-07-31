// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreServicesTypes.h"

UCloudARPin::UCloudARPin()
	: UARPin()
{
	CloudState = ECloudARPinCloudState::NotHosted;
	CloudID = FString("");
	NativeResource = nullptr;
}

FString UCloudARPin::GetCloudID()
{
	return CloudID;
}

ECloudARPinCloudState UCloudARPin::GetARPinCloudState()
{
	return CloudState;
}

void UCloudARPin::UpdateCloudState(ECloudARPinCloudState NewCloudState, FString NewCloudID)
{
	CloudState = NewCloudState;
	CloudID = NewCloudID;
}
