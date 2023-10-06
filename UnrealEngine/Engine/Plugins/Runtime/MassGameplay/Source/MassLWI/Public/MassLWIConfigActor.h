// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/LightWeightInstanceStaticMeshManager.h"
#include "MassEntityConfigAsset.h"
#include "MassLWIConfigActor.generated.h"


class UMassLWISubsystem;

UCLASS(hidecategories = (Actor, Input, Collision, Rendering, Replication, Partition, HLOD, Cooking))
class AMassLWIConfigActor : public AActor
{
	GENERATED_BODY()
public:
	virtual void PostLoad() override;

	bool IsRegisteredWithSubsystem() const { return bRegisteredWithSubsystem; }

protected:
	UPROPERTY(Category = "Derived Traits", EditAnywhere)
	FMassEntityConfig DefaultConfig;

	UPROPERTY(Category = "Derived Traits", EditAnywhere)
	TMap<TObjectPtr<UClass>, FMassEntityConfig> PerClassConfigs;

	uint8 bRegisteredWithSubsystem : 1;

	friend UMassLWISubsystem;
};
