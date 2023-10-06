// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "VirtualCameraSaveGame.h"

#include "IVirtualCameraPresetContainer.generated.h"

UINTERFACE(Blueprintable)
class VIRTUALCAMERA_API UVirtualCameraPresetContainer : public UInterface
{
	GENERATED_BODY()
};

class VIRTUALCAMERA_API IVirtualCameraPresetContainer
{
	GENERATED_BODY()
	
public:

	/**
	 * Saves a preset into the list of presets.
	 * @param bSaveCameraSettings - Should this preset save camera settings
	 * @param bSaveStabilization - Should this preset save stabilization settings
	 * @param bSaveAxisLocking - Should this preset save axis locking settings
	 * @param bSaveMotionScale - Should this preset save motion scaled settings
	 * @return the name of the preset
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Presets")
	FString SavePreset(const bool bSaveCameraSettings, const bool bSaveStabilization, const bool bSaveAxisLocking, const bool bSaveMotionScale);

	/**
	 * Loads a preset using its name as a string key.
	 * @param PresetName - The name of the preset to load
	 * @return true if successful, false otherwise
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Presets")
	bool LoadPreset(const FString& PresetName);

	/**
	 * Deletes a preset using its name as the key.
	 * @param PresetName - The name of the preset to delete
	 * @return the number of values associated with the key
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Presets")
	int32 DeletePreset(const FString& PresetName);

	/**
	 * Returns a sorted TMap of the current presets.
	 * @return a sorted TMap of settings presets
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Presets")
	TMap<FString, FVirtualCameraSettingsPreset>  GetSettingsPresets();
};
