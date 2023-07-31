// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelFromTextFactory.h"
#include "ComputeFramework/ComputeKernelFromText.h"

UComputeKernelFromTextFactory::UComputeKernelFromTextFactory()
{
	SupportedClass = UComputeKernelFromText::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UComputeKernelFromTextFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UComputeKernelFromText* Kernel = NewObject<UComputeKernelFromText>(InParent, InClass, InName, Flags);

	return Kernel;
}
