// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraPose.h"

#include "Math/Ray.h"
#include "Runtime/Engine/Classes/PhysicsField/PhysicsFieldComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraPose)

FCameraPoseFlags& FCameraPoseFlags::SetAllFlags(bool bInValue)
{
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	PropName = bInValue;

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	return *this;
}

FCameraPoseFlags& FCameraPoseFlags::ExclusiveCombine(const FCameraPoseFlags& OtherFlags)
{
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	if (OtherFlags.PropName)\
	{\
		ensureMsgf(!PropName, TEXT("Exclusive combination failed: " #PropName " set on both flags!"));\
		PropName = true;\
	}

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	return *this;
}

FCameraPoseFlags& FCameraPoseFlags::AND(const FCameraPoseFlags& OtherFlags)
{
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	PropName = PropName && OtherFlags.PropName;

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	return *this;
}

FCameraPoseFlags& FCameraPoseFlags::OR(const FCameraPoseFlags& OtherFlags)
{
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	PropName = PropName || OtherFlags.PropName;

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	return *this;
}

FCameraPose::FCameraPose()
{
}

void FCameraPose::Reset(bool bSetAllChangedFlags)
{
	*this = FCameraPose();

	if (bSetAllChangedFlags)
	{
		SetAllChangedFlags();
	}
}

void FCameraPose::SetAllChangedFlags()
{
	ChangedFlags.SetAllFlags(true);
}

void FCameraPose::ClearAllChangedFlags()
{
	ChangedFlags.SetAllFlags(false);
}

FTransform3d FCameraPose::GetTransform() const
{
	FTransform3d Transform;
	Transform.SetLocation(Location);
	Transform.SetRotation(Rotation.Quaternion());
	return Transform;
}

void FCameraPose::SetTransform(FTransform3d Transform)
{
	SetLocation(Transform.GetLocation());
	SetRotation(Transform.GetRotation().Rotator());
}

float FCameraPose::GetEffectiveFieldOfView() const
{
	checkf(
			(FocalLength > 0.f && FieldOfView <= 0.f) || (FocalLength <= 0.f && FieldOfView > 0.f),
			TEXT("FocalLength or FieldOfView must have a valid, positive value."));

	if (FocalLength > 0.f)
	{
		// Compute FOV with similar code to UCineCameraComponent...
		float CropedSensorWidth = SensorWidth * SqueezeFactor;
		if (AspectRatio > 0.0f)
		{
			float DesqueezeAspectRatio = SensorWidth * SqueezeFactor / SensorHeight;
			if (AspectRatio < DesqueezeAspectRatio)
			{
				CropedSensorWidth *= AspectRatio / DesqueezeAspectRatio;
			}
		}

		return FMath::RadiansToDegrees(2.f * FMath::Atan(CropedSensorWidth / (2.f * FocalLength)));
	}
	else
	{
		// Let's use the FOV directly, like in the good old times.
		return FieldOfView;
	}
}

FRay3d FCameraPose::GetAimRay() const
{
	const bool bDirectionIsNormalized = false;
	const FVector3d TargetDir{ TargetDistance, 0, 0 };
	return FRay3d(Location, Rotation.RotateVector(TargetDir), bDirectionIsNormalized);
}

void FCameraPose::OverrideChanged(const FCameraPose& OtherPose)
{
	const FCameraPoseFlags& OtherPoseChangedFlags = OtherPose.GetChangedFlags();

#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
	if (OtherPoseChangedFlags.PropName)\
	{\
		Set##PropName(OtherPose.Get##PropName());\
	}

UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY
}

void FCameraPose::LerpChanged(const FCameraPose& ToPose, float Factor)
{
	FCameraPoseFlags DummyFlags;
	DummyFlags.SetAllFlags(true);
	InternalLerpChanged(ToPose, Factor, DummyFlags, false, DummyFlags);
}

void FCameraPose::LerpChanged(const FCameraPose& ToPose, float Factor, const FCameraPoseFlags& InMask, bool bInvertMask, FCameraPoseFlags& OutMask)
{
	InternalLerpChanged(ToPose, Factor, InMask, bInvertMask, OutMask);
}

void FCameraPose::InternalLerpChanged(const FCameraPose& ToPose, float Factor, const FCameraPoseFlags& InMask, bool bInvertMask, FCameraPoseFlags& OutMask)
{
	if (UNLIKELY(Factor == 0.f))
	{
		return;
	}

	const bool bIsOverride = (Factor == 1.f);
	const FCameraPoseFlags& ToPoseChangedFlags = ToPose.GetChangedFlags();

	if (bIsOverride)
	{
		// The interpolation factor is 1 so we just override the properties.
	
#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
		if ((!bInvertMask && InMask.PropName) || (bInvertMask && !InMask.PropName))\
		{\
			if (ToPoseChangedFlags.PropName)\
			{\
				Set##PropName(ToPose.Get##PropName());\
				OutMask.PropName = true;\
			}\
		}

		UE_CAMERA_POSE_FOR_ALL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	}
	else
	{
		// Interpolate all the properties.
		//
		// Start with those we can simply feed to a LERP formula. Some properties don't
		// necessarily make sense to interpolate (like, what does it mean to interpolate the
		// sensor size of a camera?) but, well, we use whatever we have been given at this
		// point.

#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
		if ((!bInvertMask && InMask.PropName) || (bInvertMask && !InMask.PropName))\
		{\
			if (ToPoseChangedFlags.PropName)\
			{\
				ensureMsgf(ChangedFlags.PropName, TEXT("Interpolating " #PropName " from default value!"));\
				Set##PropName(FMath::Lerp(Get##PropName(), ToPose.Get##PropName(), Factor));\
			}\
			OutMask.PropName = true;\
		}

		UE_CAMERA_POSE_FOR_TRANSFORM_PROPERTIES()
		UE_CAMERA_POSE_FOR_INTERPOLABLE_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

		// Next, handle the special case of FOV, where we might have to blend between a pose
		// specifying FOV directly and a pose using focal length.
		if (
				(!bInvertMask && (InMask.FieldOfView || InMask.FocalLength)) ||
				(bInvertMask && (!InMask.FieldOfView || !InMask.FocalLength)))
		{
			ensureMsgf(
					ChangedFlags.FieldOfView || ChangedFlags.FocalLength, 
					TEXT("Interpolating FieldOfView or FocalLength from default value!"));
			const float FromFieldOfView = GetEffectiveFieldOfView();
			const float ToFieldOfView = ToPose.GetEffectiveFieldOfView();
			SetFieldOfView(FMath::Lerp(FromFieldOfView, ToFieldOfView, Factor));
			SetFocalLength(-1);
			OutMask.FieldOfView = true;
			OutMask.FocalLength = true;
		}

		// Last, do booleans, which just flip their value once we reach 50% interpolation.

#define UE_CAMERA_POSE_FOR_PROPERTY(PropType, PropName)\
		if ((!bInvertMask && InMask.PropName) || (bInvertMask && !InMask.PropName))\
		{\
			if (ToPoseChangedFlags.PropName && Factor >= 0.5f)\
			{\
				ensureMsgf(ChangedFlags.PropName, TEXT("Interpolating " #PropName " from default value!"));\
				Set##PropName(ToPose.Get##PropName());\
			}\
			OutMask.PropName = true;\
		}

		UE_CAMERA_POSE_FOR_BOOL_PROPERTIES()

#undef UE_CAMERA_POSE_FOR_PROPERTY

	}
}

