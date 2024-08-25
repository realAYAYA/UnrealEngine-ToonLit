// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "AvaTagCollectionFactory.generated.h"

UCLASS()
class UAvaTagCollectionFactory : public UFactory
{
	GENERATED_BODY()

	UAvaTagCollectionFactory();

	//~ Begin UFactory
	virtual uint32 GetMenuCategories() const override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
	//~ End UFactory
};
