// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyActor.h"
#include "WaterBodyLakeActor.generated.h"

class UBoxComponent;
class ULakeCollisionComponent;

// ----------------------------------------------------------------------------------

UCLASS(MinimalAPI)
class UDEPRECATED_LakeGenerator : public UDEPRECATED_WaterBodyGenerator
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<UStaticMeshComponent> LakeMeshComp;

	UPROPERTY()
	TObjectPtr<UBoxComponent> LakeCollisionComp_DEPRECATED;

	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<ULakeCollisionComponent> LakeCollision;
};

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API AWaterBodyLake : public AWaterBody
{
	GENERATED_UCLASS_BODY()
protected:
	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UDEPRECATED_LakeGenerator> LakeGenerator_DEPRECATED;
#endif
};