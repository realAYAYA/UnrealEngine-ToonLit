// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "AvaSequenceActorFactory.generated.h"

/**
 * Actor Factory with no spawn implementation for UAvaSequence
 * Used only to prevent custom UAvaSequence Asset Drag Drops from creating Level Sequence Players (e.g. when dragging from Sequence Panel)
 */
UCLASS()
class UAvaSequenceActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UAvaSequenceActorFactory();

protected:
	//~ Begin UActorFactory
	virtual bool CanCreateActorFrom(const FAssetData& InAssetData, FText& OutErrorMessage) override;
	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams) override;
	//~ End UActorFactory

	//~ Begin IAssetFactoryInterface
	virtual bool CanPlaceElementsFromAssetData(const FAssetData& InAssetData) override;
	//~ End IAssetFactoryInterface
};
