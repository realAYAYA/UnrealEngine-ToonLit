// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementsDataStorage.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "MassEntityTypes.h"
#include "Misc/CoreDelegates.h"
#include "Templates/IsPolymorphic.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "TypedElementDatabaseUI.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FTypedElementsDataStorageModule"

// MASS uses DCO in a few places, making it a difficult to consistently register Type Element's Columns and Tags
// as they may have not been set up to impersonate MASS' Fragments and Tags yet. To get around this, do the 
// registration during static initialization, which is before DCOs.
/**
 * Typed Elements provides base classes for columns and tags. These directly map to fragments and tags in MASS.
 * To avoid deep and tight coupling between both systems, columns and tags don't directly inhered from MASS, but
 * are otherwise fully compatible. To allow MASS to do its type safety checks, this class updates the type
 * information so Typed Elements columns and tags present as MASS fragments and tags from MASS's perspective.
 */
struct FTagAndFragmentImpersonators
{
	FTagAndFragmentImpersonators()
	{
		// Have FTypedElementDataStorageColumn impersonate a FMassFragment, which is the actual data storage when using MASS as a backend.
		static_assert(sizeof(FTypedElementDataStorageColumn) == sizeof(FMassFragment),
			"In order for FTypedElementDataStorageColumn to impersonate FMassFragment they need to be identical.");
		static_assert(!TIsPolymorphic<FMassFragment>::Value,
			"In order to be able to impersonate FMassFragment it can't have any virtual functions.");
		static_assert(!TIsPolymorphic<FTypedElementDataStorageColumn>::Value,
			"In order to be able to use FTypedElementDataStorageColumn to impersonate FMassFragment it can't have any virtual functions.");
		FTypedElementDataStorageColumn::StaticStruct()->SetSuperStruct(FMassFragment::StaticStruct());

		// Have FTypedElementDataStorageTag impersonate a FMassTag, which is the tag type when using MASS as a backend.
		static_assert(sizeof(FTypedElementDataStorageTag) == sizeof(FMassTag),
			"In order for FTypedElementDataStorageTag to impersonate FMassTag they need to be identical.");
		static_assert(!TIsPolymorphic<FMassTag>::Value,
			"In order to be able to impersonate FMassTag it can't have any virtual functions.");
		static_assert(!TIsPolymorphic<FTypedElementDataStorageTag>::Value,
			"In order to be able to use FTypedElementDataStorageTag to impersonate FMassTag it can't have any virtual functions.");
		FTypedElementDataStorageTag::StaticStruct()->SetSuperStruct(FMassTag::StaticStruct());
	}
};
static FTagAndFragmentImpersonators TagAndFragmentImpersonators;

void FTypedElementsDataStorageModule::StartupModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda(
		[this]()
		{
			if (!bInitialized)
			{
				Database = NewObject<UTypedElementDatabase>();
				Database->Initialize();

				DatabaseCompatibility = NewObject<UTypedElementDatabaseCompatibility>();
				DatabaseCompatibility->Initialize(Database.Get());

				DatabaseUi = NewObject<UTypedElementDatabaseUi>();
				DatabaseUi->Initialize(Database.Get());

				UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
				checkf(Registry, TEXT(
					"FTypedElementsDataStorageModule tried to register itself, but there was no Typed Element Registry to register to."));
				Registry->SetDataStorage(Database.Get());
				Registry->SetDataStorageCompatibility(DatabaseCompatibility.Get());
				Registry->SetDataStorageUi(DatabaseUi.Get());

				bInitialized = true;
			}
		});
	FCoreDelegates::OnExit.AddRaw(this, &FTypedElementsDataStorageModule::ShutdownModule);
}

void FTypedElementsDataStorageModule::ShutdownModule()
{
	if (bInitialized)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		if (Registry) // If the registry has already been destroyed there's no point in clearing the reference.
		{
			Registry->SetDataStorage(nullptr);
			Registry->SetDataStorageCompatibility(nullptr);
			Registry->SetDataStorageUi(nullptr);
		}

		if (UObjectInitialized())
		{
			DatabaseUi->Deinitialize();
			DatabaseCompatibility->Deinitialize();
			Database->Deinitialize();
		}

		bInitialized = false;
	}
}

void FTypedElementsDataStorageModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (bInitialized)
	{
		Collector.AddReferencedObject(Database);
		Collector.AddReferencedObject(DatabaseCompatibility);
		Collector.AddReferencedObject(DatabaseUi);
	}
}

FString FTypedElementsDataStorageModule::GetReferencerName() const
{
	return TEXT("Typed Elements: Data Storage Module");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTypedElementsDataStorageModule, TypedElementsDataStorage)