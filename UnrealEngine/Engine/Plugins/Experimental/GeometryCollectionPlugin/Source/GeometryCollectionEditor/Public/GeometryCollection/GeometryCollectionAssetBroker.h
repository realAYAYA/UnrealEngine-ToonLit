// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAssetBroker.h"

/** AssetBroker class for GeometryCollection assets*/
class FGeometryCollectionAssetBroker : public IComponentAssetBroker
{
public:
	// Begin IComponentAssetBroker Implementation
	UClass* GetSupportedAssetClass() override;
	virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override;
	virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override;
	// End IComponentAssetBroker Implementation
};
