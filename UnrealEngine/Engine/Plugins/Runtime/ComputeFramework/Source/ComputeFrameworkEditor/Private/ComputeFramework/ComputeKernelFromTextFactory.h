// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ComputeKernelFromTextFactory.generated.h"

UCLASS(hidecategories = Object)
class UComputeKernelFromTextFactory : public UFactory
{
	GENERATED_BODY()

public:
	UComputeKernelFromTextFactory();

	virtual UObject* FactoryCreateNew(
		UClass* InClass, 
		UObject* InParent, 
		FName InName, 
		EObjectFlags Flags, 
		UObject* Context, 
		FFeedbackContext* Warn
		) override;
};
