// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyActor.h"
#include "WaterBodyOceanActor.generated.h"

class UOceanBoxCollisionComponent;
class UOceanCollisionComponent;

// ----------------------------------------------------------------------------------

UCLASS(MinimalAPI)
class UDEPRECATED_OceanGenerator : public UDEPRECATED_WaterBodyGenerator
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(NonPIEDuplicateTransient)
	TArray<TObjectPtr<UOceanBoxCollisionComponent>> CollisionBoxes;

	UPROPERTY(NonPIEDuplicateTransient)
	TArray<TObjectPtr<UOceanCollisionComponent>> CollisionHullSets;
};

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API AWaterBodyOcean : public AWaterBody
{
	GENERATED_UCLASS_BODY()
protected:
	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UDEPRECATED_OceanGenerator> OceanGenerator_DEPRECATED;
	
	UPROPERTY()
	FVector CollisionExtents_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};