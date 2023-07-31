// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeActorFactory.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeActorFactory : public UInterchangeFactoryBase
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


