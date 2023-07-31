// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArchetype.cpp: Unreal object archetype relationship management
=============================================================================*/

#include "UObject/UObjectArchetypeInternal.h"
#include "CoreMinimal.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectAnnotation.h"
#include "Stats/StatsMisc.h"

#define UE_CACHE_ARCHETYPE (1 && !WITH_EDITORONLY_DATA)
#define UE_VERIFY_CACHED_ARCHETYPE 0

#if UE_CACHE_ARCHETYPE
struct FArchetypeInfo
{
	/**
	* default contructor
	* Default constructor must be the default item
	*/
	FArchetypeInfo()
		: ArchetypeIndex(INDEX_NONE)
	{
	}
	/**
	* Determine if this linker pair is the default
	* @return true is this is a default pair. We only check the linker because CheckInvariants rules out bogus combinations
	*/
	FORCEINLINE bool IsDefault() const
	{
		return ArchetypeIndex == INDEX_NONE;
	}

	/**
	* Constructor
	* @param InArchetype Archetype to assign
	*/
	FArchetypeInfo(int32 InArchetypeIndex)
		: ArchetypeIndex(InArchetypeIndex)
	{
	}

	int32 ArchetypeIndex;
};

static FUObjectAnnotationDense<FArchetypeInfo, true> ArchetypeAnnotation;

#endif // UE_CACHE_ARCHETYPE

UObject* GetArchetypeFromRequiredInfoImpl(const UClass* Class, const UObject* Outer, FName Name, EObjectFlags ObjectFlags, bool bUseUpToDateClass)
{
	UObject* Result = NULL;
	const bool bIsCDO = !!(ObjectFlags & RF_ClassDefaultObject);
	if (bIsCDO)
	{
		Result = bUseUpToDateClass ? Class->GetAuthoritativeClass()->GetArchetypeForCDO() : Class->GetArchetypeForCDO();
	}
	else
	{
		if (Outer
			&& Outer->GetClass() != UPackage::StaticClass()) // packages cannot have subobjects
		{
			// Get a lock on the UObject hash tables for the duration of the GetArchetype operation
			FScopedUObjectHashTablesLock HashTablesLock;

			UObject* ArchetypeToSearch = nullptr;
#if UE_CACHE_ARCHETYPE
			ArchetypeToSearch = Outer->GetArchetype();
#if UE_VERIFY_CACHED_ARCHETYPE
			{
				UObject* VerifyArchetype = GetArchetypeFromRequiredInfoImpl(Outer->GetClass(), Outer->GetOuter(), Outer->GetFName(), Outer->GetFlags(), bUseUpToDateClass);
				checkf(ArchetypeToSearch == VerifyArchetype, TEXT("Cached archetype mismatch, expected: %s, cached: %s"), *GetFullNameSafe(VerifyArchetype), *GetFullNameSafe(ArchetypeToSearch));
			}
#endif // UE_VERIFY_CACHED_ARCHETYPE
#else
			ArchetypeToSearch = GetArchetypeFromRequiredInfoImpl(Outer->GetClass(), Outer->GetOuter(), Outer->GetFName(), Outer->GetFlags(), bUseUpToDateClass);
#endif // UE_CACHE_ARCHETYPE
			UObject* MyArchetype = static_cast<UObject*>(FindObjectWithOuter(ArchetypeToSearch, Class, Name));
			if (MyArchetype)
			{
				Result = MyArchetype; // found that my outers archetype had a matching component, that must be my archetype
			}
			else if (!!(ObjectFlags & RF_InheritableComponentTemplate) && Outer->IsA<UClass>())
			{
				const UClass* OuterSuperClass = static_cast<const UClass*>(Outer)->GetSuperClass();
				for (const UClass* SuperClassArchetype = bUseUpToDateClass && OuterSuperClass ? OuterSuperClass->GetAuthoritativeClass() : OuterSuperClass;
					SuperClassArchetype && SuperClassArchetype->HasAllClassFlags(CLASS_CompiledFromBlueprint);
					SuperClassArchetype = SuperClassArchetype->GetSuperClass())
				{
					if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
					{
						if (SuperClassArchetype->HasAnyFlags(RF_NeedLoad))
						{
							UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching supers for an archetype of %s in %s"), *GetFullNameSafe(ArchetypeToSearch), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
						}
					}
					Result = static_cast<UObject*>(FindObjectWithOuter(SuperClassArchetype, Class, Name));
					// We can have invalid archetypes halfway through the hierarchy, keep looking if it's pending kill or transient
					if (IsValid(Result) && !Result->HasAnyFlags(RF_Transient))
					{
						break;
					}
				}
			}
			else
			{
				if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
				{
					if (ArchetypeToSearch->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching for an archetype of %s in %s"), *GetFullNameSafe(ArchetypeToSearch), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
					}
				}

				Result = ArchetypeToSearch->GetClass()->FindArchetype(Class, Name);
			}
		}

		if (!Result)
		{
			// nothing found, I am not a CDO, so this is just the class CDO
			Result = bUseUpToDateClass ? Class->GetAuthoritativeClass()->GetDefaultObject() : Class->GetDefaultObject();
		}
	}

	if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
	{
		if (Result && Result->HasAnyFlags(RF_NeedLoad))
		{
			UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when being set up as an archetype of %s in %s"), *GetFullNameSafe(Result), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
		}
	}

	return Result;
}

void CacheArchetypeForObject(UObject* Object, UObject* Archetype)
{
#if UE_CACHE_ARCHETYPE
#if UE_VERIFY_CACHED_ARCHETYPE
	bool bUseUpToDateClass = false;
	UObject* VerifyArchetype = GetArchetypeFromRequiredInfoImpl(Object->GetClass(), Object->GetOuter(), Object->GetFName(), Object->GetFlags(), bUseUpToDateClass);
	checkf(Archetype == VerifyArchetype, TEXT("Cached archetype mismatch, expected: %s, cached: %s"), *GetFullNameSafe(VerifyArchetype), *GetFullNameSafe(Archetype));
#endif
	ArchetypeAnnotation.AddAnnotation(Object, GUObjectArray.ObjectToIndex(Archetype));
#endif
}

UObject* UObject::GetArchetypeFromRequiredInfo(const UClass* Class, const UObject* Outer, FName Name, EObjectFlags ObjectFlags)
{
	bool bUseUpToDateClass = false;
#if WITH_EDITOR
	// While compiling we just want to use whatever is in the object hierarchy,
	// as some instances within the hierarchy may also be compiling:
	bUseUpToDateClass = GIsReinstancing && Class->GetAuthoritativeClass() == Class;
#endif
	return GetArchetypeFromRequiredInfoImpl(Class, Outer, Name, ObjectFlags, bUseUpToDateClass);
}

//DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("UObject::GetArchetype"), STAT_FArchiveRealtimeGC_GetArchetype, STATGROUP_GC);

UObject* UObject::GetArchetype() const
{
	//SCOPE_SECONDS_ACCUMULATOR(STAT_FArchiveRealtimeGC_GetArchetype);

#if UE_CACHE_ARCHETYPE
	UObject* Archetype = nullptr;
	int32 ArchetypeIndex = ArchetypeAnnotation.GetAnnotation(this).ArchetypeIndex;
	if (ArchetypeIndex == INDEX_NONE)
	{
		Archetype = GetArchetypeFromRequiredInfo(GetClass(), GetOuter(), GetFName(), GetFlags());
		// If the Outer is pending load we can't cache the archetype as it may be inacurate
		if (Archetype && !(GetOuter() && GetOuter()->HasAnyFlags(RF_NeedLoad)))
		{
			ArchetypeAnnotation.AddAnnotation(this, GUObjectArray.ObjectToIndex(Archetype));
		}
	}		
	else
	{
		FUObjectItem* ArchetypeItem = GUObjectArray.IndexToObject(ArchetypeIndex);
		check(ArchetypeItem != nullptr);
		Archetype = static_cast<UObject*>(ArchetypeItem->Object);
#if UE_VERIFY_CACHED_ARCHETYPE
		UObject* ExpectedArchetype = GetArchetypeFromRequiredInfo(GetClass(), GetOuter(), GetFName(), GetFlags());
		if (ExpectedArchetype != Archetype)
		{
			UE_LOG(LogClass, Fatal, TEXT("Cached archetype mismatch, expected: %s, cached: %s"), *ExpectedArchetype->GetFullName(), *Archetype->GetFullName());
		}
#endif // UE_VERIFY_CACHED_ARCHETYPE
	}
	// Note that IsValidLowLevelFast check may fail during initial load as not all classes are initialized at this point so skip it
	check(Archetype == nullptr || GIsInitialLoad || Archetype->IsValidLowLevelFast());

	return Archetype;
#else
	return GetArchetypeFromRequiredInfo(GetClass(), GetOuter(), GetFName(), GetFlags());
#endif // UE_CACHE_ARCHETYPE
}

/** Removes all cached archetypes to avoid doing it in static exit where it may cause crashes */
void CleanupCachedArchetypes()
{
#if UE_CACHE_ARCHETYPE
	ArchetypeAnnotation.RemoveAllAnnotations();
#endif
}
