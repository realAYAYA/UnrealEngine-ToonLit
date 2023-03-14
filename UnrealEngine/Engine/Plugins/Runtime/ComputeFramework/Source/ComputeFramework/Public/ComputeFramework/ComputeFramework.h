// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

DECLARE_LOG_CATEGORY_EXTERN(LogComputeFramework, Log, All);

namespace ComputeFramework
{
	/** Returns true if ComputeFramework is supported on a platform. */
	COMPUTEFRAMEWORK_API bool IsSupported(EShaderPlatform ShaderPlatform);

	/** Returns true if ComputeFramework is currently enabled. */
	COMPUTEFRAMEWORK_API bool IsEnabled();

	/** Rebuild all compute graphs. */
	COMPUTEFRAMEWORK_API void RebuildComputeGraphs();

	/** Tick shader compilation. */
	COMPUTEFRAMEWORK_API void TickCompilation(float DeltaSeconds);
}
