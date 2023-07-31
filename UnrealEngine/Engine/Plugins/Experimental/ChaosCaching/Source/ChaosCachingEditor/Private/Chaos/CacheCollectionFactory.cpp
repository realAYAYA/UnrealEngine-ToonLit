// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CacheCollectionFactory.h"

#include "Chaos/CacheCollection.h"

UCacheCollectionFactory::UCacheCollectionFactory()
{
	SupportedClass = UChaosCacheCollection::StaticClass();
}

bool UCacheCollectionFactory::CanCreateNew() const
{
	return true;
}

bool UCacheCollectionFactory::FactoryCanImport(const FString& Filename)
{
	return false;
}

UObject* UCacheCollectionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
												   UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UChaosCacheCollection>(InParent, InClass, InName, Flags);
}

bool UCacheCollectionFactory::ShouldShowInNewMenu() const
{
	return true;
}

bool UCacheCollectionFactory::ConfigureProperties()
{
	return true;
}
