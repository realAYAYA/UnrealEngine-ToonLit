// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"

#include "RemoteControlLogicConfig.generated.h"

/** Config class for Remote Control Logic related settings.
* Supports configuration of a valid list of Controller types.
*/
UCLASS(config=RemoteControl)
class REMOTECONTROLLOGIC_API URemoteControlLogicConfig: public UObject
{
	GENERATED_BODY()

public:
	/** PropertyBag types that are supported for use as Controllers */
	UPROPERTY(config)
	TArray<EPropertyBagPropertyType> SupportedControllerTypes;

	/** Certain types such as FVector / FRotator / FColor are specializations of UStructProperty
	* This config represents the list of such types that may be used as Controllers*/
	UPROPERTY(config)
	TArray<FName> SupportedControllerStructTypes;
};
