// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "LightWeightInstanceFactory.generated.h"

class ALightWeightInstanceManager;

UCLASS(hidecategories=Object)
class UNREALED_API ULightWeightInstanceFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface

protected:
	virtual ALightWeightInstanceManager* MakeNewLightWeightInstanceManager(UObject* InParent, FName Name, EObjectFlags Flags);

	// The parent class of the created blueprint
	UPROPERTY()
	TSubclassOf<class UObject> ParentClass;
};
