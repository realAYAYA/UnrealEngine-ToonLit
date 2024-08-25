// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Transform.h"

#include "CameraPose.generated.h"

#define UE_CAMERA_POSE_FOR_TRANSFORM_PROPERTIES()\
	UE_CAMERA_POSE_FOR_PROPERTY(FVector, Location)\
	UE_CAMERA_POSE_FOR_PROPERTY(FRotator3d, Rotation)

#define UE_CAMERA_POSE_FOR_INTERPOLABLE_PROPERTIES()\
	UE_CAMERA_POSE_FOR_PROPERTY(double, TargetDistance)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  Aperture)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  FocusDistance)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  SensorWidth)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  SensorHeight)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  SqueezeFactor)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  AspectRatio)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  NearClippingPlane)\
	UE_CAMERA_POSE_FOR_PROPERTY(float,  FarClippingPlane)

#define UE_CAMERA_POSE_FOR_FOV_PROPERTIES()\
	UE_CAMERA_POSE_FOR_PROPERTY(float, FieldOfView)\
	UE_CAMERA_POSE_FOR_PROPERTY(float, FocalLength)

#define UE_CAMERA_POSE_FOR_BOOL_PROPERTIES()\
	UE_CAMERA_POSE_FOR_PROPERTY(bool, bConstrainAspectRatio)

#define UE_CAMERA_POSE_FOR_ALL_PROPERTIES()\
	UE_CAMERA_POSE_FOR_TRANSFORM_PROPERTIES()\
	UE_CAMERA_POSE_FOR_INTERPOLABLE_PROPERTIES()\
	UE_CAMERA_POSE_FOR_FOV_PROPERTIES()\
	UE_CAMERA_POSE_FOR_BOOL_PROPERTIES()

/**
 * Boolean flags for each of the properties inside FCameraPose.
 */
struct FCameraPoseFlags
{
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	bool PropName = false;

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

public:

	/** Sets all flags to the given value. */
	FCameraPoseFlags& SetAllFlags(bool bInValue);
	/** Sets the flags that are set in OtherFlags, but checks that no flag is set on both structures. */
	FCameraPoseFlags& ExclusiveCombine(const FCameraPoseFlags& OtherFlags);

	/** Combines the flags with an AND logical operation. */
	FCameraPoseFlags& AND(const FCameraPoseFlags& OtherFlags);
	/** Combines the flags with an OR logical operation. */
	FCameraPoseFlags& OR(const FCameraPoseFlags& OtherFlags);
};

/**
 * Structure describing the state of a camera.
 *
 * Fields are private and can only be accessed via the getters and setters.
 * The ChangedFlags structure keeps track of which fields were changed via the setters.
 */
USTRUCT()
struct FCameraPose
{
	GENERATED_BODY()

public:

	FCameraPose();

	/** Resets this camera pose to its default values. */
	void Reset(bool bSetAllChangedFlags = true);

public:

	// Getters and setters

#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	PropType Get##PropName() const\
	{\
		return PropName;\
	}\
	void Set##PropName(TCallTraits<PropType>::ParamType InValue)\
	{\
		ChangedFlags.PropName = true;\
		PropName = InValue;\
	}

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

public:

	// Changed flags management

	/** Get the changed flags. */
	FCameraPoseFlags& GetChangedFlags() { return ChangedFlags; }
	/** Get the changed flags. */
	const FCameraPoseFlags& GetChangedFlags() const { return ChangedFlags; }

	/** Set the changed flags. */
	void SetChangedFlags(const FCameraPoseFlags& InChangedFlags) { ChangedFlags = InChangedFlags; }
	/** Set all fields as changed. */
	void SetAllChangedFlags();
	/** Set all fields as clean. */
	void ClearAllChangedFlags();

public:

	// Utility

	/** Gets the transform of the camera. */
	FTransform3d GetTransform() const;
	/** Sets the transform of the camera. */
	void SetTransform(FTransform3d Transform);

	/**
	 * Computes the field of view of the camera.
	 * The effective field of view can be driven by the FieldOfView property, or
	 * the FocalLength property in combination with the sensor size.
	 */
	float GetEffectiveFieldOfView() const;

	/**
	 * Gets the aiming ray of the camera.
	 */
	FRay3d GetAimRay() const;

public:

	// Interpolation
	
	/** Takes all changed properties from OtherPose and sets them on this camera pose. */
	void OverrideChanged(const FCameraPose& OtherPose);
	/** Interpolates all changed properties from ToPose using the given factor. */
	void LerpChanged(const FCameraPose& ToPose, float Factor);
	/** Interpolates changed properties from ToPose using the given factor. Only properties defined by InMask are taken into account. */
	void LerpChanged(const FCameraPose& ToPose, float Factor, const FCameraPoseFlags& InMask, bool bInvertMask, FCameraPoseFlags& OutMask);

private:

	void InternalLerpChanged(const FCameraPose& ToPose, float Factor, const FCameraPoseFlags& InMask, bool bInvertMask, FCameraPoseFlags& OutMask);

private:

	/** The location of the camera in the world */
	UPROPERTY()
	FVector3d Location = {0, 0, 0};

	/** The rotation of the camera in the world */
	UPROPERTY()
	FRotator3d Rotation = {0, 0, 0};

	/** Distance to the target */
	UPROPERTY()
	double TargetDistance = 100.0;

	/**
	 * The horizontal field of view of the camera, in degrees 
	 * If zero or less, focal length is used instead
	 */
	UPROPERTY()
	float FieldOfView = -1.f;  // Default to using a focal length

	/**
	 * The aspect ratio of the camera
	 * If zero or less, the sensor width and height are used instead
	 */
	UPROPERTY()
	float AspectRatio = -1.f;

	/**
	 * The focal length of the camera's lens, in millimeters
	 * If zero or less, field of view is used instead
	 */
	UPROPERTY()
	float FocalLength = 35.f;

	/** The aperture of the camera's lens, in f-stops */
	UPROPERTY()
	float Aperture = 2.8f;

	/** The focus distance of the camera's lens, in world units */
	UPROPERTY()
	float FocusDistance = -1.f;

	/** The width of the camera's sensor, in millimeters */
	UPROPERTY()
	float SensorWidth = 24.89f;

	/** The height of the camera's sensor, in millimeters */
	UPROPERTY()
	float SensorHeight = 18.67f;

	/** Squeeze factor for anamorphic lenses */
	UPROPERTY()
	float SqueezeFactor = 1.f;

	/** The distance to the near clipping plane, in world units */
	UPROPERTY()
	float NearClippingPlane = 10.f;

	/** The distance to the far clipping plane, in world units */
	UPROPERTY()
	float FarClippingPlane = -1.f;

	/** Whether to constrain aspect ratio */
	UPROPERTY()
	bool bConstrainAspectRatio = false;

private:
	
	/**
	 * Flags keeping track of which properties were written to since
	 * last time the flags were cleared.
	 */
	FCameraPoseFlags ChangedFlags;
};

