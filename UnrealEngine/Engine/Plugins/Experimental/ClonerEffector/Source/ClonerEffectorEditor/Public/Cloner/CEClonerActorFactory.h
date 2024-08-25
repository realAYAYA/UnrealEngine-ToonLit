// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "CEClonerActorFactory.generated.h"

UCLASS(MinimalAPI)
class UCEClonerActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UCEClonerActorFactory();

	//~ Begin UActorFactory
	virtual void PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	//~ End UActorFactory
};
