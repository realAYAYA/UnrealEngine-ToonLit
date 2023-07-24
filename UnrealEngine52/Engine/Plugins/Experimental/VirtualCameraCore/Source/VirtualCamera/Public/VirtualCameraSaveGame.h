// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SaveGame.h"
#include "Math/UnitConversion.h"
#include "VirtualCameraSaveGame.generated.h"

UENUM()
enum class EVirtualCameraAxis : uint8
{
	/* Locking for x location movement; Truck */
	LocationX,

	/* Locking for y location movement; Dolly */
	LocationY,

	/* Locking for z location movement; Boom */
	LocationZ,

	/* Locking for x rotation movement; Dutch or Roll */
	RotationX,

	/* Locking for y rotation movement; Tilt or Pitch */
	RotationY,

	/* Locking for Z rotation movement; Pan or Yaw */
	RotationZ
};

/**
 * Stores relevant information for an axis for the virtual camera pawn
 */
USTRUCT()
struct FVirtualCameraAxisSettings
{
	GENERATED_BODY()

	/** If an axis is locked, then that axis's movement will be disabled. When unlocking an axis the movement is updated as if the axis had never been locked. */
	UPROPERTY()
	bool bIsLocked;

	/** If an axis is frozen, then that frozen axis's movement will be disabled. When frozen axis is unlocked the movement is not updated, and all of the axis movement is applied as if it was in the location when the freeze was initiated. */
	UPROPERTY()
	bool bIsFrozen;

	/** The amount of stabilization that can be applied to an axis */
	UPROPERTY()
	float StabilizationScale;

	/** The scale that a user's movement should be adjusted by in game */
	UPROPERTY()
	float MovementScale;

	/** The lock offset associated with an axis */
	float LockRotationOffset;
	FVector LockLocationOffset;

	/** The freeze offset associated with an axis */
	float FreezeRotationOffset;
	FVector FreezeLocationOffset;

	FVirtualCameraAxisSettings()
	{
		bIsLocked = false;
		bIsFrozen = false;
		StabilizationScale = .5f;
		MovementScale = 1.f;
		LockRotationOffset = 0.f;
		LockLocationOffset = FVector::ZeroVector;
		FreezeRotationOffset = 0.f;
		FreezeLocationOffset = FVector::ZeroVector;
	}

	/**
	 * Add an offset based on the freeze and lock states
	 * @param InOffset - The offset to be added
	 */
	void AddRotationOffset(const float InOffset)
	{
		if (bIsFrozen)
		{
			FreezeRotationOffset += InOffset;
		}
		else if (bIsLocked)
		{
			LockRotationOffset += InOffset;
		}
	}

	/**
	 * Add an offset based on the freeze and lock states
	 * @param InOffset - The offset to be added
	 */
	void AddLocationOffset(const FVector InOffset)
	{
		if (bIsFrozen)
		{
			FreezeLocationOffset += InOffset;
		}
		else if (bIsLocked)
		{
			LockLocationOffset += InOffset;
		}
	}

	/**
	 * Get the current total offset applied to this axis
	 * @return - The total offset this axis has applied
	 */
	float GetRotationOffset() const { return FreezeRotationOffset + LockRotationOffset; }
	FVector GetLocationOffset() const { return FreezeLocationOffset + LockLocationOffset; }

	/** 
	 * Helper method to check if an axis is prevented from moving.
	 */
	bool IsAxisImmobilized() const
	{
		return bIsLocked || bIsFrozen;
	}

	/**
	 * Set the current lock state
	 * @param bNewIsLocked - The desired lock state
	 */
	void SetIsLocked(const bool bNewIsLocked)
	{
		// If unlocking, clear locking offset
		if (!bNewIsLocked)
		{
			LockRotationOffset = 0.f;
			LockLocationOffset = FVector::ZeroVector;
		}

		bIsLocked = bNewIsLocked;
	}

	/**
	 * Toggle the current lock state
	 * @return - The current lock state after toggle
	 */ 
	bool ToggleLock()
	{
		SetIsLocked(!bIsLocked);
		return bIsLocked;
	}
};

/**
 * Stores specific camera settings to be retrieved at a later time
 */
USTRUCT(BlueprintType)
struct FVirtualCameraSettings
{
	GENERATED_BODY()

	/* The focal length of the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	float FocalLength;

	/* The aperture of the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	float Aperture;

	UPROPERTY(EditAnywhere, Category = Basic)
	FString FilmbackName;

	/* The filmback sensor width of the camera */
	UPROPERTY(EditAnywhere, Category = Basic)
	float FilmbackWidth;
	
	/* The filmback sensor height of the camera */
	UPROPERTY(EditAnywhere, Category = Basic)
	float FilmbackHeight;

	UPROPERTY(EditAnywhere, Category = Basic)
	float MatteOpacity;
	
	/* The Axis settings for locking, stabilization, and scaling */
	UPROPERTY(EditAnywhere, Category = Basic)
	TMap<EVirtualCameraAxis, FVirtualCameraAxisSettings> AxisSettings;

	UPROPERTY(EditAnywhere, Category = Basic)
	bool bAllowFocusVisualization;

	UPROPERTY(EditAnywhere, Category = Basic)
	FColor DebugFocusPlaneColor;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	EUnit DesiredDistanceUnits;

	FVirtualCameraSettings()
	{
		FocalLength = 35.f;
		Aperture = 2.8f;
		FilmbackName = "";
		FilmbackWidth = 24.889f;
		FilmbackHeight = 14.f;
		bAllowFocusVisualization = true;
		DebugFocusPlaneColor = FColor(EForceInit::ForceInit);
		MatteOpacity = .7f;
		DesiredDistanceUnits = EUnit::Meters;

		AxisSettings.Add(EVirtualCameraAxis::LocationX);
		AxisSettings.Add(EVirtualCameraAxis::LocationY);
		AxisSettings.Add(EVirtualCameraAxis::LocationZ);
		AxisSettings.Add(EVirtualCameraAxis::RotationX);
		AxisSettings.Add(EVirtualCameraAxis::RotationY);
		AxisSettings.Add(EVirtualCameraAxis::RotationZ);
	}
};

/**
 * Keeps a saved location and the name of that location
 */
USTRUCT(BlueprintType)
struct FVirtualCameraWaypoint
{
	GENERATED_BODY()

	/* An associated name for the waypoint provided by the user */
	UPROPERTY(BlueprintReadWrite, Category = Basic)
	FString Name;

	/* The location of the waypoint in world space */
	UPROPERTY(BlueprintReadWrite, Category = Basic)
	FTransform WaypointTransform;

	UPROPERTY(BlueprintReadWrite, Category = Basic)
	FDateTime DateCreated;

	UPROPERTY(BlueprintReadWrite, Category = Basic)
	bool bIsHomeWaypoint;

	UPROPERTY(BlueprintReadWrite, Category = Basic)
	bool bIsFavorited;

	static int32 NextIndex;

	FVirtualCameraWaypoint()
	{
		DateCreated = FDateTime(0);
		bIsHomeWaypoint = false;
		bIsFavorited = false;
		Name = "";
	};
};

/**
* Keeps track of all the data associated with a screenshot that was taken by the user
*/
USTRUCT(BlueprintType)
struct FVirtualCameraScreenshot
{
	GENERATED_BODY()

	/* The name and location of the screenshot */
	UPROPERTY(BlueprintReadWrite, Category = Basic)
	FVirtualCameraWaypoint Waypoint;

	/* The associated camera data from when the screenshot was taken */
	UPROPERTY(BlueprintReadWrite, Category = Basic)
	FVirtualCameraSettings CameraSettings;

	static int32 NextIndex;
};

/**
 * Keeps track of all data associated with settings presets.
 */
USTRUCT(BlueprintType)
struct FVirtualCameraSettingsPreset
{
	GENERATED_BODY()

	/* Checks which settings are saved for the preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	bool bIsCameraSettingsSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	bool bIsStabilizationSettingsSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	bool bIsAxisLockingSettingsSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	bool bIsMotionScaleSettingsSaved;

	/* Checks if saettings is set as favorite */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Basic)
	bool bIsFavorited;

	UPROPERTY(EditAnywhere, Category = Basic)
	FVirtualCameraSettings CameraSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Basic)
	FDateTime DateCreated;

	static int32 NextIndex;

	FVirtualCameraSettingsPreset()
	{
		bIsCameraSettingsSaved = false;
		bIsStabilizationSettingsSaved = false;
		bIsAxisLockingSettingsSaved = false;
		bIsMotionScaleSettingsSaved = false;
		bIsFavorited = false;
		DateCreated = FDateTime(0);
	};
};

UCLASS()
class VIRTUALCAMERA_API UVirtualCameraSaveGame : public USaveGame
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = Basic)
	TMap<FString, FVirtualCameraWaypoint> Waypoints;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	TMap<FString, FVirtualCameraScreenshot> Screenshots;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	FString HomeWaypointName;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	FVirtualCameraSettings CameraSettings;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	TMap<FString, FVirtualCameraSettingsPreset> SettingsPresets;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	FString SaveSlotName;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	uint32 UserIndex;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	int32 WaypointIndex;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	int32 ScreenshotIndex;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	int32 PresetIndex;
};
