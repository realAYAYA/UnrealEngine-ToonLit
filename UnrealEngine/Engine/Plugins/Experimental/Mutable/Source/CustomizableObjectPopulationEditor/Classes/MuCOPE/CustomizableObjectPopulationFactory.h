// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectPopulationFactory.generated.h"

class FFeedbackContext;
class UClass;
class UObject;


UCLASS(MinimalAPI)
class UCustomizableObjectPopulationFactory : public UFactory
{
	GENERATED_BODY()

	UCustomizableObjectPopulationFactory();

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
	// End UFactory Interface

};
