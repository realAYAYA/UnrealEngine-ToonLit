// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "SmartObjectDefinitionFactory.generated.h"

/**
 * Factory responsible to create SmartObjectDefinitions
 */
UCLASS()
class SMARTOBJECTSEDITORMODULE_API USmartObjectDefinitionFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

protected:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
