// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphFactory.h"
#include "PCGGraph.h"

/////////////////////////
// PCGGraph
/////////////////////////

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

/////////////////////////
// PCGGraphInstance
/////////////////////////

UPCGGraphInstanceFactory::UPCGGraphInstanceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPCGGraphInstance::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPCGGraphInstanceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPCGGraphInstance* PCGGraphInstance = NewObject<UPCGGraphInstance>(InParent, InClass, InName, Flags);

	if (ParentGraph)
	{
		PCGGraphInstance->SetGraph(ParentGraph);
	}
	
	return PCGGraphInstance;
}

bool UPCGGraphInstanceFactory::ShouldShowInNewMenu() const
{
	return true;
}