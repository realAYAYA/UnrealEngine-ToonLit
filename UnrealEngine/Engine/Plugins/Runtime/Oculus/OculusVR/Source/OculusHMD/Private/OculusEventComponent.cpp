// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusEventComponent.h"
#include "OculusHMD.h"
#include "OculusDelegates.h"

void UDEPRECATED_UOculusEventComponent::OnRegister()
{
	Super::OnRegister();

	FOculusEventDelegates::OculusDisplayRefreshRateChanged.AddUObject(this, &UDEPRECATED_UOculusEventComponent::OculusDisplayRefreshRateChanged_Handler);
}

void UDEPRECATED_UOculusEventComponent::OnUnregister()
{
	Super::OnUnregister();

	FOculusEventDelegates::OculusDisplayRefreshRateChanged.RemoveAll(this);
}
