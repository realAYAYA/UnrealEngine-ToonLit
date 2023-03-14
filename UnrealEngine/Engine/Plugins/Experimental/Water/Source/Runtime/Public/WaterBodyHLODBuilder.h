// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WorldPartition/HLOD/HLODBuilder.h"
#include "WaterBodyHLODBuilder.generated.h"

UCLASS(HideDropdown)
class UWaterBodyHLODBuilder : public UHLODBuilder
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	virtual uint32 ComputeHLODHash(const UActorComponent* InSourceComponent) const override;

	/**
	 * Components created with this method need to be properly outered & assigned to your target actor.
	 */
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const override;
#endif
};
