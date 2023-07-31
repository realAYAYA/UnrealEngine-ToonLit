// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/PhysicsVolume.h"
#include "WaterBodyActor.h"
#include "WaterBodyExclusionVolume.generated.h"

/**
 * WaterBodyExclusionVolume allows players not enter surface swimming when touching a water volume
 */
UCLASS()
class WATER_API AWaterBodyExclusionVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

public:
	void UpdateOverlappingWaterBodies();

#if WITH_EDITOR
	void UpdateActorIcon();
#endif // WITH_EDITOR

protected:
	virtual void PostLoad() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetCustomIconName() const override;
#endif // WITH_EDITOR

public:
	/** If checked, all water bodies overlapping with this exclusion volumes will be affected. */
	UPROPERTY(EditAnywhere, Category = Water)
	bool bExcludeAllOverlappingWaterBodies = true;

	/** List of water bodies that will be affected by this exclusion volume */
	UPROPERTY(EditInstanceOnly, Category = Water, meta = (EditCondition = "!bExcludeAllOverlappingWaterBodies"))
	TArray<TObjectPtr<AWaterBody>> WaterBodiesToExclude;

#if WITH_EDITORONLY_DATA
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
