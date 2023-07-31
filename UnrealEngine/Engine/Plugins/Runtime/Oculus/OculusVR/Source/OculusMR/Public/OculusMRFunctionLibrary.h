// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OculusMRFunctionLibrary.generated.h"

class USceneComponent;
class UDEPRECATED_UOculusMR_Settings;
struct FTrackedCamera;

namespace OculusHMD
{
	class FOculusHMD;
}

UCLASS(deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSMR_API UDEPRECATED_UOculusMRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	// Get the OculusMR settings object
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DisplayName = "Get Oculus MR Settings", DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static UDEPRECATED_UOculusMR_Settings* GetOculusMRSettings();

	// Get the component that the OculusMR camera is tracking. When this is null, the camera will track the player pawn.
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static USceneComponent* GetTrackingReferenceComponent();
 
	// Set the component for the OculusMR camera to track. If this is set to null, the camera will track the player pawn.
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool SetTrackingReferenceComponent(USceneComponent* Component);

	// Get the scaling factor for the MRC configuration. Returns 0 if not available.
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DisplayName = "Get MRC Scaling Factor", DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static float GetMrcScalingFactor();

	// Set the scaling factor for the MRC configuration. This should be a positive value set to the same scaling as the VR player pawn so that the game capture and camera video are aligned.
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DisplayName = "Set MRC Scaling Factor", DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool SetMrcScalingFactor(float ScalingFactor = 1.0f);

	// Check if MRC is enabled
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsMrcEnabled();

	// Check if MRC is enabled and actively capturing
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsMrcActive();

public:

	static class OculusHMD::FOculusHMD* GetOculusHMD();

	/** Retrieve an array of all (calibrated) tracked cameras which were calibrated through the CameraTool */
	static void GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly = true);

	static bool GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation);
};
