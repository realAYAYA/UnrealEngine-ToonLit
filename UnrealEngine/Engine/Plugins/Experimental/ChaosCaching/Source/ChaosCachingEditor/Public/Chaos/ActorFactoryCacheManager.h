// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"

#include "ActorFactoryCacheManager.generated.h"

class AActor;
struct FAssetData;

UCLASS()
class CHAOSCACHINGEDITOR_API UActorFactoryCacheManager : public UActorFactory
{
	GENERATED_BODY()

	UActorFactoryCacheManager();

	//~ Begin UActorFactory Interface
	virtual bool     CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void     PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	//~ End UActorFactory Interface
};
