// Copyright Epic Games, Inc. All Rights Reserved.

#include "Directors/SingleCameraDirector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SingleCameraDirector)

USingleCameraDirector::USingleCameraDirector(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
}

void USingleCameraDirector::OnRun(const FCameraDirectorRunParams& Params, FCameraDirectorRunResult& OutResult)
{
	if (CameraMode)
	{
		OutResult.ActiveCameraModes.Add(CameraMode);
	}
}

