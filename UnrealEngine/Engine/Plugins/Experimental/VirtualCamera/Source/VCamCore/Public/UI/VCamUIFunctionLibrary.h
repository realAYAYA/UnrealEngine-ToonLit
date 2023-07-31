// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/VCamConnectionStructs.h"

#include "VCamUIFunctionLibrary.generated.h"

class UInputAction;
class UVCamModifier;

/*
 * Function Library primarily used for exposing functionality of UI related structs to Blueprints 
 */
UCLASS()
class VCAMCORE_API UVCamUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/*
	 * Checks whether a given VCam Connection is successfully connected.
	 * If this returns true then it is safe to assume that the Connected Modifier for the Connection is valid to use
	 */
	UFUNCTION(BlueprintPure, Category = "VCam Connections", meta=(DisplayName = "Is Connected"))
	static bool IsConnected_VCamConnection(const FVCamConnection& Connection);

	/*
	 * Gets the name of the associated Connection Point for a given VCam Connection.
	 * This will return "None" if the IsConnected() is false
	 */
	UFUNCTION(BlueprintPure, Category = "VCam Connections", meta = (DisplayName = "Get Connection Point Name"))
	static FName GetConnectionPointName_VCamConnection(const FVCamConnection& Connection);

	/*
	 * Gets the Connected Modifier for the given VCam Connection
	 * The returned Modifier is guaranteed to be valid if IsConnected() is true
	 */
	UFUNCTION(BlueprintPure, Category = "VCam Connections", meta = (DisplayName = "Get Connected Modifier"))
	static UVCamModifier* GetConnectedModifier_VCamConnection(const FVCamConnection& Connection);

	/*
	 * Gets the Connected Input Action for the given VCam Connection
	 * The returned Input Action is only guaranteed to be valid if both IsConnected() is true AND the VCam Connection is set to require an Input Action
	 * If the VCam Connection doesn't require an Input Action then this may still return a valid Action if the Connection Point
	 * had an associated Input Action however you will need to test for this manually
	 */
	UFUNCTION(BlueprintPure, Category = "VCam Connections", meta = (DisplayName = "Get Connected Input Action"))
	static UInputAction* GetConnectedInputAction_VCamConnection(const FVCamConnection& Connection);
};