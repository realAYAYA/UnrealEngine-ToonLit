// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiPlatformTargetReceiptBuildWorkers.h"

FMultiPlatformTargetReceiptBuildWorkers::FMultiPlatformTargetReceiptBuildWorkers(const TCHAR* TargetReceiptFilePath)
: bAllPlatformsInitialized(false)
{
	FString PathString = TargetReceiptFilePath;
	if (!PathString.Contains(TEXT("$(Platform)")))
	{
		new(&PlatformSpecificWorkerFactories[0]) FTargetReceiptBuildWorker(*PathString);
		return;
	}

	new(&PlatformSpecificWorkerFactories[SupportedPlatform_Win64]) FTargetReceiptBuildWorker(*PathString.Replace(TEXT("$(Platform)"), TEXT("Win64")));
	new(&PlatformSpecificWorkerFactories[SupportedPlatform_Mac]) FTargetReceiptBuildWorker(*PathString.Replace(TEXT("$(Platform)"), TEXT("Mac")));
	new(&PlatformSpecificWorkerFactories[SupportedPlatform_Linux]) FTargetReceiptBuildWorker(*PathString.Replace(TEXT("$(Platform)"), TEXT("Linux")));
	bAllPlatformsInitialized = true;
}

FMultiPlatformTargetReceiptBuildWorkers::~FMultiPlatformTargetReceiptBuildWorkers()
{
	if (bAllPlatformsInitialized)
	{
		((FTargetReceiptBuildWorker*)&PlatformSpecificWorkerFactories[SupportedPlatform_Win64])->~FTargetReceiptBuildWorker();
		((FTargetReceiptBuildWorker*)&PlatformSpecificWorkerFactories[SupportedPlatform_Mac])->~FTargetReceiptBuildWorker();
		((FTargetReceiptBuildWorker*)&PlatformSpecificWorkerFactories[SupportedPlatform_Linux])->~FTargetReceiptBuildWorker();
	}
	else
	{
		((FTargetReceiptBuildWorker*)&PlatformSpecificWorkerFactories[0])->~FTargetReceiptBuildWorker();
	}
}
