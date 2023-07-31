// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerInput.h"

#include "VirtualCameraUserSettings.generated.h"

/**
 * Virtual Camera User Settings
 */
UCLASS(config=VirtualCamera, BlueprintType)
class VIRTUALCAMERA_API UVirtualCameraUserSettings : public UObject
{
	GENERATED_BODY()

public:

	UVirtualCameraUserSettings();
	
	/** Controls interpolation speed when smoothing when changing focus distance. This is used to set the value of FocusSmoothingInterpSpeed in the Virtual camera CineCamera component */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (ClampMin = "1.0", ClampMax = "50.0", DisplayName = "Focus Interpolation Speed"))
	float FocusInterpSpeed = 8.0f;

	/** Controls how fast the camera moves when using joysticks */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Joysticks Speed"))
	float JoysticksSpeed = 5.0f;

	/** Sets the maximum possible joystick speed */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Max Joysticks Speed"))
	float MaxJoysticksSpeed = 10.0f;

	/** Whether the map is displayed using grayscale or full color */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Display Map In Grayscale"))
	bool bIsMapGrayscale = true;
	
	/** Whether to change camera lens and fstop when teleporting to a screenshot to those with which the screenshot was taken */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Override Camera Settings On Teleporting To Screenshot"))
	bool bOverrideCameraSettingsOnTeleportToScreenshot = true;

	/** Stores the filmback preset name selected by the user */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Virtual Camera Filmback"))
	FString VirtualCameraFilmback;

	/** Whether to display film leader when recording a take */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Display Film Leader"))
	bool bDisplayFilmLeader = true;

	/** Whether to teleport to the home bookmark when VCam starts */
	UPROPERTY(EditAnywhere, config, Category = "VirtualCamera", meta = (DisplayName = "Teleport To Home On Start"))
	bool bTeleportOnStart = true;

	/** Default Vcam Class for Vcam Operator Panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "VirtualCamera|Presets")
	TSoftClassPtr<AActor> DefaultVCamClass;

	UPROPERTY(BlueprintReadWrite, config, Category = "VirtualCamera", meta = (DisplayName = "VirtualCamera Axis Mappings"))
	TArray<struct FInputAxisKeyMapping> AxisMappings;

	UPROPERTY(BlueprintReadWrite, config, Category = "VirtualCamera", meta = (DisplayName = "VirtualCamera Action Mappings"))
	TArray<struct FInputActionKeyMapping> ActionMappings;

	/** Get FocusInterpSpeed variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	float GetFocusInterpSpeed();

	/** Set FocusInterpSpeed variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetFocusInterpSpeed(const float InFocusInterpSpeed);

	/** Get JoysticksSpeed variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	float GetJoysticksSpeed();

	/** Set JoysticksSpeed variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetJoysticksSpeed(const float InJoysticksSpeed);

	/** Get MaxJoysticksSpeed variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	float GetMaxJoysticksSpeed();

	/** Set MaxJoysticksSpeed variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetMaxJoysticksSpeed(const float InMaxJoysticksSpeed);

	/** Get bIsMapGrayscale variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool IsMapGrayscle();

	/** Set bIsMapGrayscale variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetIsMapGrayscle(const bool bInIsMapGrayscle);

	/** Get bOverrideCameraSettingsOnTeleportToScreenshot variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool GetShouldOverrideCameraSettingsOnTeleport();

	/** Set bOverrideCameraSettingsOnTeleportToScreenshot variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetShouldOverrideCameraSettingsOnTeleport(const bool bInOverrideCameraSettings);

	/** Get VirtualCameraFilmback variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	FString GetSavedVirtualCameraFilmbackPresetName();

	/** Set VirtualCameraFilmback variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetSavedVirtualCameraFilmbackPresetName(const FString& InFilmback);

	/** Get bDisplayFilmLeader variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool GetShouldDisplayFilmLeader();

	/** Set bDisplayFilmLeader variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetShouldDisplayFilmLeader(const bool bInDisplayFilmLeader);

	/** Get bTeleportOnStart variable */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool GetTeleportOnStart();

	/** Set bTeleportOnStart variable */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetTeleportOnStart(const bool bInTeleportOnStart);

	/** Fills the Axis/Action mappings with assosiated gamepad bindings */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void InjectGamepadKeybinds(); 
	
	/** Retrieve all VirtualCamera action mappings by a certain name. */
	UFUNCTION(BlueprintPure, Category = Settings)
	void GetActionMappingsByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const;

	/** Retrieve all VirtualCamera axis mappings by a certain name. */
	UFUNCTION(BlueprintPure, Category = Settings)
	void GetAxisMappingsByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const;

};