// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeFactoryBase.h"

#include "InterchangeLightActorFactory.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeLightActorFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;

	virtual UObject* CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams) override;

	virtual bool CanExecuteOnAnyThread() const override
	{
		return false;
	}

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////
};