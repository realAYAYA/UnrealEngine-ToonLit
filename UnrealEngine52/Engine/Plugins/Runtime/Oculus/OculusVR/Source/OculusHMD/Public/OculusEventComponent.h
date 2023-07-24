// Copyright Epic Games, Inc. All Rights Reserved.
// OculusEventComponent.h: Component to handle receiving events from Oculus HMDs

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "OculusEventComponent.generated.h"

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent, DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."), ClassGroup = OculusHMD, deprecated)
class OCULUSHMD_API UDEPRECATED_UOculusEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOculusDisplayRefreshRateChangedEventDelegate, float, fromRefreshRate, float, toRefreshRate);

	UPROPERTY(BlueprintAssignable)
	FOculusDisplayRefreshRateChangedEventDelegate OculusDisplayRefreshRateChanged;

	void OnRegister() override;
	void OnUnregister() override;

private:
	/** Native handlers that get registered with the actual FCoreDelegates, and then proceed to broadcast to the delegates above */
	void OculusDisplayRefreshRateChanged_Handler(float fromRefresh, float toRefresh) { OculusDisplayRefreshRateChanged.Broadcast(fromRefresh, toRefresh); }
};