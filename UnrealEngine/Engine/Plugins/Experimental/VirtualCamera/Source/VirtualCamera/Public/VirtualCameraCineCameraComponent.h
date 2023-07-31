// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "IVirtualCameraController.h"

#include "VirtualCameraCineCameraComponent.generated.h"

UCLASS(Blueprintable, BlueprintType)
class VIRTUALCAMERA_API UVirtualCameraCineCameraComponent : public UCineCameraComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** List of preset aperture options, aperture will always be one of these values */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	TArray<float> ApertureOptions;

	/** List of preset focal length options, focal length will be one of these values, unless manually zooming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	TArray<float> FocalLengthOptions;

	/** List of preset matte options to chose from, UI options will only pull from this, unless a filmback option with a custom matte is selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	TArray<float> MatteOptions;

	/** List of preset filmback options, filmback will always be one of these values */
	UPROPERTY(EditAnywhere, Category = "Current Camera Settings")
	TMap<FString, FCameraFilmbackSettings> FilmbackOptions;

	/** The desired filmback settings to be shown in the viewport within Virtual Camera UI window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Current Camera Settings")
	FCameraFilmbackSettings DesiredFilmbackSettings;

	/** The filmback settings to be used for additional letterboxing if desired */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Current Camera Settings")
	float MatteAspectRatio;

	/** The opacity of the matte in the camera view */
	UPROPERTY(BlueprintReadWrite, Category = "Current Camera Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MatteOpacity;

	/** The X and Y ratios of Desired View Size to Actual View Size (calculated as Desired/ Actual) */
	UPROPERTY(BlueprintReadWrite, Category = "Current Camera Settings")
	FVector2D ViewSizeRatio;

	/** Tracks if autofocus is enabled */
	bool bAutoFocusEnabled;

	/** Tracks whether or not the camera's view needs to be restored */
	bool bCameraViewResetNeeded;

	virtual void BeginPlay() override;

	/**
	 * Get the current focal length value on the camera.
	 * @return the current focal length of the camera being viewed
	 */
	float GetCurrentFocalLength() const { return CurrentFocalLength; }

	/**
	 * Sets the current focal length of the cinematic camera to a given value.
	 * @param NewFocalLength - The new target focal length for the camera
	 */
	void SetCurrentFocalLength(const float NewFocalLength) { CurrentFocalLength = NewFocalLength; }

	/**
	 * Adjust the focal length of the camera to an adjacent preset value.
	 * @param bShiftUp - If true, the focal length will increase; if false, it will decrease
	 * @return the new focal length of the camera in mm
	 */
	float ChangeFocalLengthPreset(const bool bShiftUp);

	/**
	 * Get the current aperture value on the camera.
	 * @return the current aperture of the camera component
	 */
	float GetCurrentAperture() const { return CurrentAperture; }

	/**
	 * Set the current aperture value on the camera.
	 * @param NewAperture - The new aperture value
	 */
	void SetCurrentAperture(const float NewAperture) { CurrentAperture = NewAperture; }

	/**
	 * Adjusts the aperture of the camera to an adjacent preset value.
	 * @param bShiftUp - If true, the aperture will increase; if false, it will decrease
	 * @return the new aperture of the camera
	 */
	float ChangeAperturePreset(const bool bShiftUp);

	/**
	 * Get the name of the current preset filmback option on the camera.
	 * @return the name of the current preset filmback option
	 */
	FString GetCurrentFilmbackName() const { return CurrentFilmbackOptionsKey; };

	/**
	 * Get the currently used focus method.
	 * @return the Current focus method
	 */
	EVirtualCameraFocusMethod GetCurrentFocusMethod() const { return CurrentFocusMethod; };

	/**
	 * Stores the names of all available filmback presets into an array.
	 * @param OutFilmbackPresets - Upon return, will contain all available filmback presets.
	 * @return true if operation was successful
	 */
	bool GetFilmbackPresetOptions(TArray<FString>& OutFilmbackPresetsArray) const;

	/**
	 * Set the filmback settings to a new filmback preset.
	 * @param NewFilmbackPreset - The name of the desired preset to use
	 * @return true if operation was successful; false if NewFilmbackPreset is not a valid option
	 */
	bool SetFilmbackPresetOption(const FString& NewFilmbackPreset);

	/**
	 * Returns the values of all matte options.
	 * @param &OutMatteValues - Upon return, array will contain all matte values.
	 */
	void GetMatteValues(TArray<float>& OutMatteValues) const ;

	/**
	 * Returns the current matte aspect ratio.
	 * @return the current matte setting
	 */
	float GetMatteAspectRatio() { return MatteAspectRatio; }

	/**
	 * Set the matte aspect ratio to a new value.
	 * @param NewMatteAspectRatio - The desired matte aspect to use
	 * @return true if operation was successful; false if NewMattePreset is not a valid input
	 */
	bool SetMatteAspectRatio(const float NewMatteAspectRatio);

	/**
	 * Sets the camera focus method.
	 * @param NewFocusMethod - The new focus method to be used by the camera
	 */
	void SetFocusMethod(const EVirtualCameraFocusMethod NewFocusMethod);

	/**
	 * Sets the focus change rate.
	 * @param NewFocusChangeSmoothness - The new change rate to use for smooth camera focus shifting (Clamped between 0 and 1)
	 */
	void SetFocusChangeSmoothness(const float NewFocusChangeSmoothness);

	/**
	 * Gets the current focus distance of the camera.
	 * @return the focal distance of the camera
	 */
	float GetCurrentFocusDistance() { return CurrentFocusDistance; }

	/**
	 * Sets the current focus distance of the camera to a new value.
	 * @param NewFocusDistance - The new focus distance for the camera
	 */
	void SetCurrentFocusDistance(const float NewFocusDistance) { FocusSettings.ManualFocusDistance = NewFocusDistance; }

	/**
	 * Sets the current focus distance to a new value based on current focus settings.
	 * @param NewFocusDistance - The desired focus distance
	 */
	void SetFocusDistance(const float NewFocusDistance);

	/**
	 * Sets tracked actor settings on the camera.
	 * @param ActorToTrack - The actor that the focus should follow
	 * @param TrackingPointOffset - The offset from the root of the actor to the point to focus on
	 */
	void SetTrackedActorForFocus(AActor* ActorToTrack, const FVector TrackingPointOffset);

	/**
	 * Adds a blendable object to the camera's post process settings.
	 * @param BlendableToAdd - The blendable that will be added to the post process settings
	 * @param Weight - The weighting of the blendable's alpha
	 */
	void AddBlendableToCamera(const TScriptInterface<IBlendableInterface> BlendableToAdd, const float Weight);

	/**
	 * Toggles focus visualization tools on camera.
	 * @param bShowFocusVisualization - The desired state of the visualization tools
	 */
	void SetFocusVisualization(bool bShowFocusVisualization);

	/**
	 * Is focus visualization tools activated on camera.
	 */
	bool IsFocusVisualizationActivated() const;

	/**
	 * Updates the camera view to have the desired film format view within the Virtual Camera UI area.
	 */
	void UpdateCameraView();

	/**
	 * Allow camera view updates.
	 */
	void AllowCameraViewUpdates() { bAllowCameraViewUpdates = true; }

	/**
	 * Disable camera view updates.
	 */
	void StopCameraViewUpdates() { bAllowCameraViewUpdates = false; }

protected:
	/** The current filmback option preset being used */
	FString CurrentFilmbackOptionsKey;

	/** Mesh used for the focus plane */
	UStaticMesh* FocusPlaneMesh;

	/** How smooth focus changes are, set by user through UI */
	float FocusChangeSmoothness;

	/** Saves the current focus method for reset if menus is exited without applying settings */
	EVirtualCameraFocusMethod CachedFocusMethod;

	/** The current focus method */
	EVirtualCameraFocusMethod CurrentFocusMethod;

	/** Whether or not camera view updates should occur */
	bool bAllowCameraViewUpdates;

	/**
	 * Searches a preset array for the closest value to it.
	 * @param SearchValue - The value that will be used to find the closest preset
	 * @return the index of the closest value to the search value in the preset array
	 */
	int32 FindClosestPresetIndex(const TArray<float>& ArrayToSearch, const float SearchValue) const;
};
