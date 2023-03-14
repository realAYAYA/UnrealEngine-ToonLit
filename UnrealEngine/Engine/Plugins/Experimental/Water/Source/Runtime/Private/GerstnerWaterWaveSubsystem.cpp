// Copyright Epic Games, Inc. All Rights Reserved.

#include "GerstnerWaterWaveSubsystem.h"
#include "GerstnerWaterWaveViewExtension.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GerstnerWaterWaveSubsystem)

UGerstnerWaterWaveSubsystem::UGerstnerWaterWaveSubsystem()
{
}

void UGerstnerWaterWaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UGerstnerWaterWaveSubsystem::BeginFrameCallback);
}


void UGerstnerWaterWaveSubsystem::Deinitialize()
{
	GerstnerWaterWaveViewExtensions.Empty();

	FCoreDelegates::OnBeginFrame.RemoveAll(this);

	Super::Deinitialize();
}

void UGerstnerWaterWaveSubsystem::Register(FGerstnerWaterWaveViewExtension* InViewExtension)
{
	check(!GerstnerWaterWaveViewExtensions.Contains(InViewExtension));
	GerstnerWaterWaveViewExtensions.Add(InViewExtension);
}

void UGerstnerWaterWaveSubsystem::Unregister(FGerstnerWaterWaveViewExtension* InViewExtension)
{
	check(GerstnerWaterWaveViewExtensions.Contains(InViewExtension));
	GerstnerWaterWaveViewExtensions.Remove(InViewExtension);
}

void UGerstnerWaterWaveSubsystem::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GerstnerWaterWaveViewExtensions.GetAllocatedSize());
}

void UGerstnerWaterWaveSubsystem::BeginFrameCallback()
{
	// In case there was a change, all registered view extensions need to update their GPU data : 
	if (bRebuildGPUData)
	{
		for (FGerstnerWaterWaveViewExtension* GerstnerWaterWaveViewExtension : GerstnerWaterWaveViewExtensions)
		{
			GerstnerWaterWaveViewExtension->bRebuildGPUData = true;
		}
	}
	bRebuildGPUData = false;
}

