// Copyright Epic Games, Inc. All Rights Reserved.

#include "../Public/TypedElementsDataStorageRevisionControl.h"

void FTypedElementsDataStorageRevisionControlModule::StartupModule()
{
}

void FTypedElementsDataStorageRevisionControlModule::ShutdownModule()
{
}

void FTypedElementsDataStorageRevisionControlModule::AddReferencedObjects(FReferenceCollector& Collector)
{
}

FString FTypedElementsDataStorageRevisionControlModule::GetReferencerName() const
{
	return TEXT("Typed Elements: Revision Control Module");
}

IMPLEMENT_MODULE(FTypedElementsDataStorageRevisionControlModule, TypedElementsDataStorageRevisionControl)
