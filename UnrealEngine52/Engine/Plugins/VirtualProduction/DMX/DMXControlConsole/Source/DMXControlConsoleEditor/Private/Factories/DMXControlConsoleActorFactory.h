// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "UObject/ObjectMacros.h"

#include "DMXControlConsoleActorFactory.generated.h"

class AActor;
struct FAssetData;


/** Actor Factory for DMX Control Console Actor */
UCLASS()
class UDMXControlConsoleActorFactory
	: public UActorFactory
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXControlConsoleActorFactory();

	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, const FActorSpawnParameters& InSpawnParams) override;
	//~ End UActorFactory Interface
};
