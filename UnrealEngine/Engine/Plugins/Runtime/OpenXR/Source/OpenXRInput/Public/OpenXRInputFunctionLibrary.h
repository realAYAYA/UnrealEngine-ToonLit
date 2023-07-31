// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenXRInputFunctionLibrary.generated.h"

UCLASS()
class OPENXRINPUT_API UOpenXRInputFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	* Add a player mappable input config for Enhanced Input. This will make the tracking system aware of the actions and bindings that are used for XR motion controllers.
	* Attaching input configs to the session can only be done once, so if multiple input configs need to be added only check AttachToSession for the last input config.
	* 
	* @param InputConfig		The path to the player mappable input config asset
	* @param AttachToSession	Attach all pending configs to the running session
	*
	* @return			False if the input config can't be attached to the session, true otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static bool BeginXRSession(class UPlayerMappableInputConfig* InputConfig);


	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void EndXRSession();
};
