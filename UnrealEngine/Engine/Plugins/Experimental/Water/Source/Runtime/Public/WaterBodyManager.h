// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWaterBodyComponent;
class FGerstnerWaterWaveViewExtension;

class WATER_API FWaterBodyManager
{
public:
	void Initialize(UWorld* World);
	void Deinitialize();

	/** 
	 * Register any water body component upon addition to the world
	 * @param InWaterBodyComponent
	 * @return int32 the unique sequential index assigned to this water body component
	 */
	int32 AddWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent);

	/** Unregister any water body upon removal to the world */
	void RemoveWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent);

	/** Recomputes wave-related data whenever it changes on one of water bodies. */
	void RequestWaveDataRebuild();

	/** Returns the maximum of all MaxWaveHeight : */
	float GetGlobalMaxWaveHeight() const { return GlobalMaxWaveHeight; }

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	void ForEachWaterBodyComponent (TFunctionRef<bool(UWaterBodyComponent*)> Pred) const;

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	static void ForEachWaterBodyComponent (const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Pred);

	bool HasAnyWaterBodies() const { return WaterBodyComponents.Num() > 0; }

private:
	/** List of components registered to this manager. May contain nullptr indices (indicated by the UnusedWaterBodyIndices array). */
	TArray<UWaterBodyComponent*> WaterBodyComponents;
	TArray<int32> UnusedWaterBodyIndices;

	float GlobalMaxWaveHeight = 0.0f;

	TSharedPtr<FGerstnerWaterWaveViewExtension, ESPMode::ThreadSafe> GerstnerWaterWaveViewExtension;
};