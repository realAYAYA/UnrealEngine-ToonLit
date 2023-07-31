// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Pawn.h"
#include "IVirtualCameraOptions.h"
#include "IVirtualCameraPresetContainer.h"
#include "VirtualCameraSaveGame.h"
#include "VirtualCameraCineCameraComponent.h"
#include "VirtualCameraMovementComponent.h"
#include "VirtualCameraPawnBase.generated.h"

// Forward Declarations
class USceneComponent;

/**
 * A class to handle aspects of virtual Camera related to general settings, and communicating with components.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, HideCategories = ("Pawn", "Camera", "Rendering", "Replication", "Input", "Actor", "HLOD"))
class VIRTUALCAMERA_API AVirtualCameraPawnBase : public APawn, public IVirtualCameraPresetContainer, public IVirtualCameraOptions
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Root component */
	USceneComponent* DefaultSceneRoot;

	/** Cinematic camera used for view */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Cinematic Camera")
	TObjectPtr<UVirtualCameraCineCameraComponent> CineCamera;

	/** Movement Component to handle the motion input for the camera */
	UVirtualCameraMovementComponent* MovementComponent;

	/** Determines if values should be saved between sessions */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bSaveSettingsWhenClosing;

	/** Stores the name of the save slot being used currently */
	UPROPERTY(EditAnywhere, Category = "Default")
	FString SavedSettingsSlotName;

	/* Stores the list of settings presets, and saved presets */
	UPROPERTY(EditAnywhere, Category = "Default")
	TMap<FString, FVirtualCameraSettingsPreset> SettingsPresets;

	/**
	 * Returns the information associated with a waypoint.
	 * @param WaypointName - The name of the waypoint to retrieve
	 * @param OutWaypointInfo - Upon return, will hold the info for that waypoint.
	 */
	void GetWaypointInfo(const FString WaypointName, FVirtualCameraWaypoint &OutWaypointInfo) const;

	/**
	 * Collects a list of existing waypoint names into an out array.
	 * @param OutArray - Upon return, will store the waypoint names.
	 */
	void GetWaypointNames(TArray<FString>& OutArray) const;

	/**
	 * Returns the information associated with a Screenshot.
	 * @param ScreenshotName - The name of the screenshot to retrieve
	 * @param OutScreenshotInfo - Upon return, will hold the info for the screenshot.
	 */
	void GetScreenshotInfo(const FString ScreenshotName, FVirtualCameraScreenshot &OutScreenshotInfo) const;

	/**
	 * Collects a list of existing screenshot names into an out array.
	 * @param OutArray - Upon return, will store screenshot names.
	 */
	void GetScreenshotNames(TArray<FString>& OutArray) const;

	/** 
	 * Change the name of a waypoint to a different name.
	 * @param TargetWaypoint - The current name of the waypoint to be renamed
	 * @param NewWaypointName - The new name of the waypoint
	 * @return true if the renaming operation was successful, false if not
	 */
	bool RenameWaypoint(const FString& TargetWaypoint, const FString& NewWaypointName);

	/**
	 * Stores the current pawn location as a waypoint.
	 * @return the index of the new waypoint in the saved waypoints array
	 */
	FString SaveWaypoint();

	/**
	 * Saves a preset into the list of presets.
	 * @param bSaveCameraSettings - Should this preset save camera settings
	 * @param bSaveStabilization - Should this preset save stabilization settings
	 * @param bSaveAxisLocking - Should this preset save axis locking settings
	 * @param bSaveMotionScale - Should this preset save motion scaled settings
	 * @return the name of the preset
	 */
	FString SavePreset(const bool bSaveCameraSettings, const bool bSaveStabilization, const bool bSaveAxisLocking, const bool bSaveMotionScale);

	/**
	 * Returns a sorted TMap of the current presets.
	 * @return a sorted TMap of settings presets
	 */
	TMap<FString, FVirtualCameraSettingsPreset>  GetSettingsPresets();

	/**
	 * Loads a preset using its name as a string key.
	 * @param PresetName - The name of the preset to load
	 * @return true if successful, false otherwise
	 */
	bool LoadPreset(const FString& PresetName);

	/**
	 * Deletes a preset using its name as the key.
	 * @param PresetName - The name of the preset to delete
	 * @return the number of values associated with the key
	 */
	int32 DeletePreset(const FString& PresetName);

	/**
	 * Deletes a screenshot, using its name as the key.
	 * @param ScreenshotName - The name of the screenshot to delete
	 * @return the number of values associated with the key
	 */
	int32 DeleteScreenshot(const FString& ScreenshotName);

	/**
	 * Deletes a waypoint, using its name as the key.
	 * @param WaypointName - The name of the waypoint to delete
	 * @return the number of values associated with the key
	 */
	int32 DeleteWaypoint(const FString& WaypointName);

	/**
	 * Updates the current settings to reflect those in the preset.
	 * @param PresetToLoad - The preset containing the desired settings
	 */
	void UpdateSettingsFromPreset(FVirtualCameraSettingsPreset* PresetToLoad);

	/**
	 * Stores the new home location.
	 * @param NewHomeWaypointName - The name of the new home waypoint to use; Use empty string to clear
	 */
	void SaveHomeWaypoint(const FString& NewHomeWaypointName);

	/**
	 * Teleports the pawn to a location associated with the specified waypoint.
	 * @param WaypointIndex - The index of the target waypoint in the waypoints array
	 * @return true if the teleport was successful
	 */
	bool TeleportToWaypoint(const FString& WaypointName);

	/**
	 * Teleports the pawn to the current marked home waypoint.
	 * @return true if the teleport was successful
	 */
	bool TeleportToHomeWaypoint();

	/**
	 * Takes a screenshot from the current view and saves the location and camera settings.
	 * @return the index of the saved screenshot in the screenshots array
	 */
	FString TakeScreenshot();

	/**
	 * Moves a camera to the location where a screenshot was taken and restores camera settings used for that screenshot.
	 * @param ScreenshotIndex - The index of the screenshot in the Screenshots array
	 * @return whether or not the load was successful
	 */
	bool LoadScreenshotView(const FString& ScreenshotIndex);

	/**
	 * Change the name of a waypoint to a different name.
	 * @param TargetWaypoint - The current name of the waypoint to be renamed
	 * @param NewWaypointName - The new name of the waypoint
	 * @return true if the renaming operation was successful, false if not
	 */
	bool RenameScreenshotLocation(const FString& TargetWaypoint, const FString& NewWaypointName);

	/**
	 * Blueprint event to trigger the highlighting of a specific actor.
	 * @param HighlightedActor - The Actor on which the focus is set
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void HighlightTappedActor(AActor* HighlightedActor);

	/**
	 * Blueprint event to trigger focus plane visualization for a set amount of time.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void TriggerFocusPlaneTimer();

	/**
	 * Forwards any focus change commands from outside sources to the camera component.
	 * @param NewFocusDistance - The desired focus distance for the camera
	 */
	void SetFocusDistance(const float NewFocusDistance) { CineCamera->SetFocusDistance(NewFocusDistance); }

	/**
	 * Forwards tracked actor changes to the camera component to be handled.
	 * @param ActorToTrack - The actor that the focus should follow
	 * @param TrackingPointOffset - The offset from the root of the actor to the point to focus on
	 */
	void SetTrackedActorForFocus(AActor* ActorToTrack, FVector TrackingPointOffset) { CineCamera->SetTrackedActorForFocus(ActorToTrack, TrackingPointOffset); }

	/** 
	 * Sets whether settings should be saved on exit. 
	 * @param bShouldSettingsSave - Whether settings should be saved
	 */
	void SetSaveSettingsWhenClosing(const bool bShouldSettingsSave) { bSaveSettingsWhenClosing = bShouldSettingsSave; };

	/**
	 * Gets whether settings should save when closing.
	 * @return whether settings should save when closing
	 */
	bool GetSaveSettingsWhenClosing() const { return bSaveSettingsWhenClosing; };

	/**
	 * Check to see if the camera is in autofocus mode.
	 * @return true if camera should be autofocusing, false if not
	 */
	bool IsAutoFocusEnabled() const { return CineCamera->bAutoFocusEnabled; }

	/**
	 * Send any movement input data forward to the Movement Component.
	 * @param Location - The current location of the tracker
	 * @param Rotation - The current rotation of the tracker
	 */
	void ProcessMovementInput(const FVector& Location, const FRotator& Rotation) { MovementComponent->ProcessMovementInput(Location, Rotation); }

	/**
	 * Checks whether or not focus visualization can activate
	 * @return the current state of touch event visualization
	 */
	bool IsFocusVisualizationAllowed() const { return bAllowFocusVisualization; }

	/**
	 * Sets whether or not focus visualization can activate
	 * @param bShouldAllowFocusVisualization - Whether or not focus visualization should be allowed
	 */
	void SetAllowFocusPlaneVisualization(bool bShouldAllowFocusVisualization) { bAllowFocusVisualization = bShouldAllowFocusVisualization; }

	/**
	 * Blueprint event for signaling UI that game settings have been loaded.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void LoadFinished();

	/**
	 * Set the matte aspect ratio to a new value.
	 * @return DesiredUnits - The new unit to use for distance measures like focus distance
	 */
	EUnit GetDesiredDistanceUnits() const { return DesiredDistanceUnits; }

	/**
	 * Set the matte aspect ratio to a new value.
	 * @param DesiredUnits - The new unit to use for distance measures like focus distance
	 */
	void SetDesiredDistanceUnits(const EUnit DesiredUnits);

	/**
	 * Sets whether or not a preset is favorited
	 * @param PresetName - The name of the preset to adjust favorite setting for
	 * @param bIsFavorite - Whether settings should be saved
	 */
	void SetPresetFavoriteStatus(const FString& PresetName, const bool bIsFavorite);

	/**
	 * Sets whether or not a screenshot is favorited
	 * @param ScreenshotName - The name of the screenshot to adjust favorite setting for
	 * @param bIsFavorite - Whether settings should be saved
	 */
	void SetScreenshotFavoriteStatus(const FString& ScreenshotName, const bool bIsFavorite);

	/**
	 * Sets whether or not a waypoint is favorited
	 * @param WaypointName - The name of the waypoint to adjust favorite setting for
	 * @param bIsFavorite - Whether settings should be saved
	 */
	void SetWaypointFavoriteStatus(const FString& WaypointName, const bool bIsFavorite);

protected:
	/** Tracks any waypoints the player has saved for teleporting */
	TMap<FString, FVirtualCameraWaypoint> Waypoints;

	/** Stores the locations of any screenshots that were taken */
	TMap<FString, FVirtualCameraScreenshot> Screenshots;

	/** Tracks which waypoint is the "home" waypoint, defaults to first value */
	FString HomeWaypointName;

	/** The next preset number */
	static int32 PresetIndex;

	/** The next waypoint number */
	static int32 WaypointIndex;

	/** The next screenshot number */
	static int32 ScreenshotIndex;

	/** The desired unit in which to display focus distance */
	EUnit DesiredDistanceUnits;

	/** Should focus plane be shown on all touch focus events */
	UPROPERTY(BlueprintReadOnly, Category = "Default")
	bool bAllowFocusVisualization;

	/** Number formatter for Screenshots, Waypoints and Presets. */
	uint32 MinimumIntegralDigits;

	/**
	 * Stores the current camera settings to a save file for later use.
	 */
	void SaveSettings();

	/**
	 * Loads camera settings from a saved file.
	 */
	void LoadSettings();

	/**
	 * Convenience function to leftpad numbers with zeros
	 * @param InNumber - The input number to be padded
	 * @param MinNumberOfCharacters - The minimum number of characters in the returned string
	 * @return The input number padded to be at least MinNumberOfCharacters in length
	 */
	FString LeftPadWithZeros(int32 InNumber, int32 MinNumberOfCharacters) const;

	//~ Begin IVirtualCameraPresetContainer Interface
	virtual FString SavePreset_Implementation(const bool bSaveCameraSettings, const bool bSaveStabilization, const bool bSaveAxisLocking, const bool bSaveMotionScale) override;
	virtual bool LoadPreset_Implementation(const FString& PresetName) override;
	virtual int32 DeletePreset_Implementation(const FString& PresetName) override;
	virtual TMap<FString, FVirtualCameraSettingsPreset> GetSettingsPresets_Implementation() override;
	//~ End IVirtualCameraPresetContainer Interface

	//~ Begin IVirtualCameraOptions Interface
	virtual void SetDesiredDistanceUnits_Implementation(const EUnit DesiredUnits) override;
	virtual EUnit GetDesiredDistanceUnits_Implementation() override;
	virtual bool IsFocusVisualizationAllowed_Implementation() override;
	//~ End IVirtualCameraOptions Interface
};
