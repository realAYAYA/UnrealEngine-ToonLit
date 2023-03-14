// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "ProceduralFoliageVolume.generated.h"

class UProceduralFoliageComponent;
class FLoaderAdapterActor;

UCLASS()
class FOLIAGE_API AProceduralFoliageVolume: public AVolume, public IWorldPartitionActorLoaderInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = ProceduralFoliage, VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UProceduralFoliageComponent> ProceduralComponent;

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void PostRegisterAllComponents();
	//~ End AActor Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostEditImport() override;
	//~ End UObject Interface

	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;

	//~ Begin IWorldPartitionActorLoaderInterface interface
	virtual ILoaderAdapter* GetLoaderAdapter() override;
	//~ End IWorldPartitionActorLoaderInterface interface

private:
	FLoaderAdapterActor* WorldPartitionActorLoader;
#endif
};
