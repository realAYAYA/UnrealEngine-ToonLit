// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelFactory.h"
#include "ComputeFramework/ComputeKernel.h"

UComputeKernelFactory::UComputeKernelFactory()
{
	SupportedClass = UComputeKernel::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UComputeKernelFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UComputeKernel* Kernel = NewObject<UComputeKernel>(InParent, InClass, InName, Flags);

	return Kernel;
}
