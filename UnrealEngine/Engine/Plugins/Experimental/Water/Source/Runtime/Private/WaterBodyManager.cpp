// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyManager.h"
#include "WaterBodyActor.h"
#include "WaterSubsystem.h"
#include "GerstnerWaterWaveViewExtension.h"

void FWaterBodyManager::Initialize(UWorld* World)
{
	if (World != nullptr)
	{
		GerstnerWaterWaveViewExtension = FSceneViewExtensions::NewExtension<FGerstnerWaterWaveViewExtension>(World);
		GerstnerWaterWaveViewExtension->Initialize();
	}
}

void FWaterBodyManager::Deinitialize()
{
	GerstnerWaterWaveViewExtension->Deinitialize();
	GerstnerWaterWaveViewExtension.Reset();
}

int32 FWaterBodyManager::AddWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent)
{
	int32 Index = INDEX_NONE;
	if (UnusedWaterBodyIndices.Num())
	{
		Index = UnusedWaterBodyIndices.Pop();
		check(WaterBodyComponents[Index] == nullptr);
		WaterBodyComponents[Index] = InWaterBodyComponent;
	}
	else
	{
		Index = WaterBodyComponents.Add(InWaterBodyComponent);
	}

	RequestWaveDataRebuild();

	check(Index != INDEX_NONE);
	return Index;
}

void FWaterBodyManager::RemoveWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent)
{
	const int32 WaterBodyIndex = InWaterBodyComponent->GetWaterBodyIndex();
	check(WaterBodyIndex != INDEX_NONE);
	UnusedWaterBodyIndices.Add(WaterBodyIndex);
	WaterBodyComponents[WaterBodyIndex] = nullptr;

	RequestWaveDataRebuild();

	// Reset all arrays once there are no more waterbodies
	if (UnusedWaterBodyIndices.Num() == WaterBodyComponents.Num())
	{
		UnusedWaterBodyIndices.Empty();
		WaterBodyComponents.Empty();
	}
}

void FWaterBodyManager::RequestWaveDataRebuild()
{
	if (GerstnerWaterWaveViewExtension)
	{
		GerstnerWaterWaveViewExtension->bRebuildGPUData = true;
	}

	// Recompute the maximum of all MaxWaveHeight : 
	GlobalMaxWaveHeight = 0.0f;
	for (const UWaterBodyComponent* WaterBodyComponent : WaterBodyComponents)
	{
		if (WaterBodyComponent != nullptr)
		{
			GlobalMaxWaveHeight = FMath::Max(GlobalMaxWaveHeight, WaterBodyComponent->GetMaxWaveHeight());
		}
	}
}

void FWaterBodyManager::ForEachWaterBodyComponent(TFunctionRef<bool(UWaterBodyComponent*)> Pred) const
{
	for (UWaterBodyComponent* WaterBodyComponent : WaterBodyComponents)
	{
		if (WaterBodyComponent)
		{
			if (!Pred(WaterBodyComponent))
			{
				return;
			}
		}
	}
}

void FWaterBodyManager::ForEachWaterBodyComponent(const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Pred)
{
	if (FWaterBodyManager* Manager = UWaterSubsystem::GetWaterBodyManager(World))
	{
		Manager->ForEachWaterBodyComponent(Pred);
	}
}