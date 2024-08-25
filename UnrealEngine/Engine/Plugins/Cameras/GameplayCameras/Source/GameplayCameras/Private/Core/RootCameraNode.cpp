// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RootCameraNode.h"

#include "Core/CameraMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RootCameraNode)

void URootCameraNode::ActivateCameraMode(const FActivateCameraModeParams& Params)
{
	OnActivateCameraMode(Params);
}

