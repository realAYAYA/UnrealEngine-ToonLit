// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildWorkers.h"

#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/Optional.h"
#include "MultiPlatformTargetReceiptBuildWorkers.h"

struct FDerivedDataBuildWorkers
{
	FMultiPlatformTargetReceiptBuildWorkers BaseTextureBuildWorkerFactory {TEXT("$(EngineDir)/Binaries/$(Platform)/BaseTextureBuildWorker.Target")};
};

TOptional<FDerivedDataBuildWorkers> GBuildWorkers;

void InitDerivedDataBuildWorkers()
{
	if (!GBuildWorkers.IsSet())
	{
		GBuildWorkers.Emplace();
	}
}
