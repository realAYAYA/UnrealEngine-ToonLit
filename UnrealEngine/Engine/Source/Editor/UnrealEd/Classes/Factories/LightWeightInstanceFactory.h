// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "LightWeightInstanceFactory.generated.h"

class ALightWeightInstanceManager;

UCLASS(hidecategories=Object, MinimalAPI)
class ULightWeightInstanceFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	UNREALED_API virtual bool ConfigureProperties() override;
	UNREALED_API virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface

protected:

	// The parent class of the created blueprint
	UPROPERTY()
	TSubclassOf<class UObject> ParentClass;
};
