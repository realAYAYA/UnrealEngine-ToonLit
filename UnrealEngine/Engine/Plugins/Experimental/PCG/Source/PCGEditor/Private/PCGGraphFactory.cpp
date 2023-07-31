// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphFactory.h"
#include "PCGGraph.h"

UPCGGraphFactory::UPCGGraphFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPCGGraph::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPCGGraphFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPCGGraph>(InParent, InClass, InName, Flags);
}

bool UPCGGraphFactory::ShouldShowInNewMenu() const
{
	return true;
}