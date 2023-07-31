// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/ParallelFor.h"

#include "CoreTypes.h"
#include "HAL/IConsoleManager.h"

CORE_API int32 GParallelForBackgroundYieldingTimeoutMs = 8;
static FAutoConsoleVariableRef CVarParallelForBackgroundYieldingTimeout(
	TEXT("Async.ParallelFor.YieldingTimeout"),
	GParallelForBackgroundYieldingTimeoutMs,
	TEXT("The timeout (in ms) when background priority parallel for task will yield execution to give higher priority tasks the chance to run.")
);