// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusSourceFactory.h"
#include "OptimusSource.h"

UOptimusSourceFactory::UOptimusSourceFactory()
{
	SupportedClass = UOptimusSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UOptimusSourceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UOptimusSource* Kernel = NewObject<UOptimusSource>(InParent, InClass, InName, Flags);

	return Kernel;
}
