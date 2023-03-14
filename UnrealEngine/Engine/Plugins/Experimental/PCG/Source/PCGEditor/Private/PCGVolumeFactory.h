// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"

#include "PCGVolumeFactory.generated.h"

UCLASS(MinimalAPI, config=Editor)
class UPCGVolumeFactory : public UActorFactoryBoxVolume
{
	GENERATED_BODY()

public:
	UPCGVolumeFactory(const FObjectInitializer& ObjectInitializer);

	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	//~ End UActorFactory Interface
};