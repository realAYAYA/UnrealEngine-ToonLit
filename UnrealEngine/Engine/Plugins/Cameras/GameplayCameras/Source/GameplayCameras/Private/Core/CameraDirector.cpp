// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraDirector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraDirector)

void UCameraDirector::Run(const FCameraDirectorRunParams& Params, FCameraDirectorRunResult& OutResult)
{
	OnRun(Params, OutResult);
}

