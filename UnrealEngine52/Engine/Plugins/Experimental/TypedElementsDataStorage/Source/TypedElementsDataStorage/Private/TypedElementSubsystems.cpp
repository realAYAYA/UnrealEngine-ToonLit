// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementSubsystems.h"

#include "Elements/Framework/TypedElementRegistry.h"

//
// UTypedElementDataStorageSubsystem
//

UTypedElementDataStorageSubsystem::~UTypedElementDataStorageSubsystem()
{
	DataStorage = nullptr;
}

ITypedElementDataStorageInterface* UTypedElementDataStorageSubsystem::Get()
{
	if (!DataStorage)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UTypedElementDataStorageSubsystem created before the Typed Elements Registry is available."));
		DataStorage = Registry->GetMutableDataStorage();
	}
	return DataStorage;
}

const ITypedElementDataStorageInterface* UTypedElementDataStorageSubsystem::Get() const
{
	return const_cast<UTypedElementDataStorageSubsystem*>(this)->Get();
}


//
// UTypedElementDataStorageUiSubsystem
//

UTypedElementDataStorageUiSubsystem::~UTypedElementDataStorageUiSubsystem()
{
	DataStorageUi = nullptr;
}

ITypedElementDataStorageUiInterface* UTypedElementDataStorageUiSubsystem::Get()
{
	if (!DataStorageUi)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UTypedElementDataStorageUiSubsystem created before the Typed Elements Registry is available."));
		DataStorageUi = Registry->GetMutableDataStorageUi();
	}
	return DataStorageUi;
}

const ITypedElementDataStorageUiInterface* UTypedElementDataStorageUiSubsystem::Get() const
{
	return const_cast<UTypedElementDataStorageUiSubsystem*>(this)->Get();
}


//
// UTypedElementDataStorageCompatibilitySubsystem
//

UTypedElementDataStorageCompatibilitySubsystem::~UTypedElementDataStorageCompatibilitySubsystem()
{
	DataStorageCompatibility = nullptr;
}

ITypedElementDataStorageCompatibilityInterface* UTypedElementDataStorageCompatibilitySubsystem::Get()
{
	if (!DataStorageCompatibility)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UTypedElementDataStorageCompatibilitySubsystem created before the Typed Elements Registry is available."));

		DataStorageCompatibility = Registry->GetMutableDataStorageCompatibility();
	}
	return DataStorageCompatibility;
}

const ITypedElementDataStorageCompatibilityInterface* UTypedElementDataStorageCompatibilitySubsystem::Get() const
{
	return const_cast<UTypedElementDataStorageCompatibilitySubsystem*>(this)->Get();
}
