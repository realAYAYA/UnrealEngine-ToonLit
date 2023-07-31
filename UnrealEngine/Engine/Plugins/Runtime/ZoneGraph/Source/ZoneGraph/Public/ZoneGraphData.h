// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphData.generated.h"

class UZoneGraphRenderingComponent;

UCLASS(config = ZoneGraph, defaultconfig, NotBlueprintable)
class ZONEGRAPH_API AZoneGraphData : public AActor
{
	GENERATED_BODY()
public:
	AZoneGraphData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject/AActor Interface
	virtual void PostActorCreated() override;
	virtual void PostLoad() override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif // WITH_EDITOR
	//~ End UObject/AActor Interface

	FORCEINLINE bool IsDrawingEnabled() const { return bEnableDrawing; }
	void UpdateDrawing() const;

	FORCEINLINE bool IsRegistered() const { return bRegistered; }
	void OnRegistered(const FZoneGraphDataHandle DataHandle);
	void OnUnregistered();

	// TODO: I wonder if the storage is unnecessary indirection?
	FZoneGraphStorage& GetStorageMutable() { return ZoneStorage; }
	const FZoneGraphStorage& GetStorage() const { return ZoneStorage; }
	FCriticalSection& GetStorageLock() const { return ZoneStorageLock; }

	FBox GetBounds() const;

	/** @return Combined hash of all ZoneShapes that were used to build the data. */
	uint32 GetCombinedShapeHash() const { return CombinedShapeHash; }

	/** Sets Combined hash of all ZoneShapes that were used to build the data. */
	void SetCombinedShapeHash(const uint32 Hash) { CombinedShapeHash = Hash; }

protected:
	bool RegisterWithSubsystem();
	bool UnregisterWithSubsystem();

	bool bRegistered;

	/** if set to true then this zone graph data will be drawing itself when requested as part of "show navigation" */
	UPROPERTY(Transient, EditAnywhere, Category = Display)
	bool bEnableDrawing;

	UPROPERTY(transient, duplicatetransient)
	TObjectPtr<UZoneGraphRenderingComponent> RenderingComp;

	UPROPERTY()
	FZoneGraphStorage ZoneStorage;

	/** Critical section to prevent rendering of the zone graph storage data while it's getting rebuilt */
	mutable FCriticalSection ZoneStorageLock;

	/** Combined hash of all ZoneShapes that were used to build the data. */
	UPROPERTY()
	uint32 CombinedShapeHash = 0;
};
