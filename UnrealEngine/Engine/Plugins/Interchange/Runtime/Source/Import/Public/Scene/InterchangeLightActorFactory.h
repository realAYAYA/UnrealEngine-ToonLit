// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Scene/InterchangeActorFactory.h"

#include "InterchangeLightActorFactory.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeLightActorFactory : public UInterchangeActorFactory
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

protected:
	//////////////////////////////////////////////////////////////////////////
	// Interchange actor factory interface begin

	virtual UObject* ProcessActor(class AActor& SpawnedActor, const UInterchangeActorFactoryNode& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer, const FImportSceneObjectsParams& Params) override;


	// Interchange actor factory interface end
	//////////////////////////////////////////////////////////////////////////
};