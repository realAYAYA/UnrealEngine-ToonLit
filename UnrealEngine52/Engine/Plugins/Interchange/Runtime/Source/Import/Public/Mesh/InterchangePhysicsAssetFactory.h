// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangePhysicsAssetFactory.generated.h"

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangePhysicsAssetFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Physics; }
	virtual UObject* ImportAssetObject_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual UObject* ImportAssetObject_Async(const FImportAssetObjectParams& Arguments) override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////
};


