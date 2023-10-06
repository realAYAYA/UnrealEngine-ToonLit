// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraMovement.h"

UVirtualCameraMovement::UVirtualCameraMovement()
	: LocalTransform(FTransform::Identity)
	, LocalAxisTransform(FTransform::Identity)
	, CalculatedTransform(FTransform::Identity)
	, LocationScale(FVector::OneVector)
	, RotationScale(FRotator(1.f, 1.f, 1.f))
	, bLocalAxisSet(false)
{

}

void UVirtualCameraMovement::SetLocalTransform(const FTransform& InTransform)
{
	LocalTransform = InTransform;
	Update();
}

void UVirtualCameraMovement::SetLocalAxis(const FTransform& InTransform)
{
	LocalAxisTransform = InTransform;
	LocalAxisTransform.SetScale3D(FVector(1.f));

	bLocalAxisSet = true;
	Update();
}

void UVirtualCameraMovement::ResetLocalAxis()
{
	LocalAxisTransform = FTransform::Identity;
	bLocalAxisSet = false;
	Update();
}

void UVirtualCameraMovement::SetLocationScale(FVector InLocationScale)
{
	LocationScale = InLocationScale;
	Update();
}

void UVirtualCameraMovement::SetRotationScale(FRotator InRotationScale)
{
	RotationScale = InRotationScale;
	Update();
}

void UVirtualCameraMovement::Update()
{
	CalculatedTransform = LocalTransform;

	FVector TransformedLocation = LocalAxisTransform.InverseTransformPosition(LocalTransform.GetLocation());
	TransformedLocation *= LocationScale;
	CalculatedTransform.SetLocation(LocalAxisTransform.TransformPosition(TransformedLocation));

	FQuat TransformedRotation = LocalAxisTransform.InverseTransformRotation(LocalTransform.GetRotation());
	FRotator TransformedRotator = TransformedRotation.Rotator();
	TransformedRotator.Pitch *= RotationScale.Pitch;
	TransformedRotator.Yaw *= RotationScale.Yaw;
	TransformedRotator.Roll *= RotationScale.Roll;
	TransformedRotator.Normalize();
	LocalTransform.GetRotation().Rotator().SetClosestToMe(TransformedRotator);
	CalculatedTransform.SetRotation(LocalAxisTransform.TransformRotation(TransformedRotator.Quaternion()));
}
