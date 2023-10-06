// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
