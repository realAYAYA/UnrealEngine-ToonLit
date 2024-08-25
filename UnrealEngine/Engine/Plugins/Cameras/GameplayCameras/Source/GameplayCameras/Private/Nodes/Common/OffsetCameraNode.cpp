// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Common/OffsetCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OffsetCameraNode)

void UOffsetCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	const FRotator3d Rotation = OutResult.CameraPose.GetRotation();
	const FVector3d LocalOffset = Rotation.RotateVector(Offset);

	const FVector3d Location = OutResult.CameraPose.GetLocation();
	OutResult.CameraPose.SetLocation(Location + LocalOffset);
}

