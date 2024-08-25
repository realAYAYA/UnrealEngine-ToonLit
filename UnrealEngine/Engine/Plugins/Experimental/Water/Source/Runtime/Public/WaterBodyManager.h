// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class UWaterBodyComponent;
class AWaterZone;
class FWaterViewExtension;

DECLARE_MULTICAST_DELEGATE_OneParam(FWaterBodyEvent, UWaterBodyComponent*);


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

	int32 AddWaterZone(AWaterZone* InWaterZone);
	void RemoveWaterZone(AWaterZone* InWaterZone);

	/** Recomputes water gpu data whenever it changes on one of the managed water types. */
	void RequestGPUDataRebuild();

	/** Recomputes wave-related data whenever it changes on one of water bodies. */
	void RequestWaveDataRebuild();

	/** Returns the maximum of all MaxWaveHeight : */
	float GetGlobalMaxWaveHeight() const { return GlobalMaxWaveHeight; }

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	void ForEachWaterBodyComponent (TFunctionRef<bool(UWaterBodyComponent*)> Pred) const;

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	static void ForEachWaterBodyComponent (const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Pred);

	void ForEachWaterZone(TFunctionRef<bool(AWaterZone*)> Pred) const;
	static void ForEachWaterZone(const UWorld* World, TFunctionRef<bool(AWaterZone*)> Pred);

	bool HasAnyWaterBodies() const { return WaterBodyComponents.Num() > 0; }

	int32 NumWaterBodies() const { return WaterBodyComponents.Num(); }
	int32 MaxWaterBodyIndex() const { return WaterBodyComponents.GetMaxIndex(); }

	/** Shrinks the sparse array storage for water body components and water zones. Ensures that MaxIndex == MaxAllocatedIndex */
	void Shrink();

	int32 NumWaterZones() const { return WaterZones.Num(); }

	FWaterViewExtension* GetWaterViewExtension() { return WaterViewExtension.Get(); }

	FWaterBodyEvent OnWaterBodyAdded;

	FWaterBodyEvent OnWaterBodyRemoved;

private:
	/** List of components registered to this manager. */
	TSparseArray<UWaterBodyComponent*> WaterBodyComponents;

	/** List of Water zones registered to this manager. */
	TSparseArray<AWaterZone*> WaterZones;

	float GlobalMaxWaveHeight = 0.0f;

	TSharedPtr<FWaterViewExtension, ESPMode::ThreadSafe> WaterViewExtension;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
