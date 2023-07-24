// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "OptimusSourceFactory.generated.h"

UCLASS(hidecategories = Object)
class UOptimusSourceFactory : public UFactory
{
	GENERATED_BODY()

public:
	UOptimusSourceFactory();

	virtual UObject* FactoryCreateNew(
		UClass* InClass, 
		UObject* InParent, 
		FName InName, 
		EObjectFlags Flags, 
		UObject* Context, 
		FFeedbackContext* Warn
		) override;
};
