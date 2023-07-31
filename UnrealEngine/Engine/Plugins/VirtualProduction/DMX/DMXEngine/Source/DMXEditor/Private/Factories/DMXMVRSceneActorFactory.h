// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "UObject/ObjectMacros.h"

#include "DMXMVRSceneActorFactory.generated.h"

class AActor;
struct FAssetData;


UCLASS()
class UDMXMVRSceneActorFactory
	: public UActorFactory
{
	GENERATED_BODY()

public:
	UDMXMVRSceneActorFactory();

	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, const FActorSpawnParameters& InSpawnParams) override;
	//~ End UActorFactory Interface
};
