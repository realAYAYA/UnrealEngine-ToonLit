// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ComputeKernelFactory.generated.h"

UCLASS(hidecategories = Object)
class UComputeKernelFactory : public UFactory
{
	GENERATED_BODY()

public:
	UComputeKernelFactory();

	virtual UObject* FactoryCreateNew(
		UClass* InClass, 
		UObject* InParent, 
		FName InName, 
		EObjectFlags Flags, 
		UObject* Context, 
		FFeedbackContext* Warn
		) override;
};
