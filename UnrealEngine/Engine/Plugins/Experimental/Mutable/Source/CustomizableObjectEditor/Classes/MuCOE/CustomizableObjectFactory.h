// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectFactory.generated.h"

class FFeedbackContext;
class UClass;
class UObject;


UCLASS(MinimalAPI)
class UCustomizableObjectFactory : public UFactory
{
	GENERATED_BODY()

	UCustomizableObjectFactory();

	// Begin UObject Interface
	virtual bool ConfigureProperties() override;

	// Begin UFactory Interface
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
};
