// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PhysicsVolume.h"
#include "WaterBodyExclusionVolume.generated.h"

class AWaterBody;

UENUM()
enum class EWaterExclusionMode
{
	/**
	 * Adds all water bodies specified in the WaterBodies list to the exclusion volume.
	 * If none are specified, no water body overlapped by this volume will be part of the exclusion.
	 */
	AddWaterBodiesListToExclusion,
	/**
	 * Removes all water bodies specified in the WaterBodies list from the exclusion volume.
	 * If none are specified, every water body overlapped by this volume will be part of the exclusion.
	 */
	RemoveWaterBodiesListFromExclusion,
};

struct FWaterExclusionVolumeChangedParams
{
	FWaterExclusionVolumeChangedParams(const FPropertyChangedEvent& InPropertyChangedEvent = FPropertyChangedEvent(/*InProperty = */nullptr))
		: PropertyChangedEvent(InPropertyChangedEvent)
	{}

	/** Provides some additional context about how the water exclusion volume data has changed (property, type of change...) */
	FPropertyChangedEvent PropertyChangedEvent;

	/** Indicates user initiated Parameter change */
	bool bUserTriggered = false;
};

/**
 * WaterBodyExclusionVolume allows players not enter surface swimming when touching a water volume
 */
UCLASS()
class WATER_API AWaterBodyExclusionVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

public:
	void UpdateOverlappingWaterBodies(const FWaterExclusionVolumeChangedParams& Params);

	UE_DEPRECATED(5.4, "UpdateOverlappingWaterBodies not taking a params struct is deprecated. The function now has the potential of dirtying water bodies when Params.bUserTriggered = true!")
	void UpdateOverlappingWaterBodies();
#if WITH_EDITOR
	void UpdateActorIcon();
#endif // WITH_EDITOR

	virtual void PostRegisterAllComponents() override;
protected:
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetCustomIconName() const override;
#endif // WITH_EDITOR

	/** Updates all water bodies affected by this exclusion volume to rebuild due to a change in this exclusion volume. */
	void UpdateAffectedWaterBodyCollisions(const FWaterExclusionVolumeChangedParams& Params);
public:
	/** Determines the behavior of the WaterBodies list. */
	UPROPERTY(EditAnywhere, Category = Water)
	EWaterExclusionMode ExclusionMode = EWaterExclusionMode::RemoveWaterBodiesListFromExclusion;

	/** List of water bodies that will be added or removed from the exclusion volume based on the ExclusionMode parameter. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = Water, meta = (DisplayAfter=ExclusionMode))
	TArray<TSoftObjectPtr<AWaterBody>> WaterBodies;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (DeprecationMessage = "Property changed from boolean to EWaterExclusionMode enum. a value of AddWaterBodiesListToExclusion is equivalent to a value of false."))
	bool bExcludeAllOverlappingWaterBodies_DEPRECATED = true;

	UPROPERTY(meta = (DeprecationMessage = "Property renamed to WaterBodies"))
	TArray<TObjectPtr<AWaterBody>> WaterBodiesToExclude_DEPRECATED;

	UPROPERTY(meta = (DeprecationMessage = "Property renamed to bExcludeAllOverlapping"))
	bool bIgnoreAllOverlappingWaterBodies_DEPRECATED = false;

	UPROPERTY(meta = (DeprecationMessage = "Property renamed to WaterBodiesToExclude"))
	TArray<TObjectPtr<AWaterBody>> WaterBodiesToIgnore_DEPRECATED;

	UPROPERTY(meta = (DeprecatedProperty))
	TObjectPtr<AWaterBody> WaterBodyToIgnore_DEPRECATED;

	UPROPERTY(Transient)
	TObjectPtr<class UBillboardComponent> ActorIcon;
#endif // WITH_EDITORONLY_DATA
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "WaterBodyActor.h"
#endif
