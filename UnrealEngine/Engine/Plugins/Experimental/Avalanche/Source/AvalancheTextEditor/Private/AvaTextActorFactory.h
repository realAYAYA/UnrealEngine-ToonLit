// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "AvaTextActorFactory.generated.h"

UCLASS()
class UAvaTextActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UAvaTextActorFactory();

	//~ Begin UActorFactory
	virtual void PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	//~ End UActorFactory
};
