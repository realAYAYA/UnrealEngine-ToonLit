// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Memento/TypedElementMementoSystem.h"
#include "UObject/UObjectGlobals.h"

#include "TypedElementObjectReinstancingManager.generated.h"

class ITypedElementDataStorageCompatibilityInterface;
class ITypedElementDataStorageInterface;
class UTypedElementDatabaseCompatibility;
class UTypedElementMementoSystem;

UCLASS(Transient)
class UTypedElementObjectReinstancingManager : public UObject
{
	GENERATED_BODY()
public:
	UTypedElementObjectReinstancingManager();

	void Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility, UTypedElementMementoSystem& InMementoSystem);
	void Deinitialize();

private:
	void RegisterQueries();
	void UnregisterQueries();
	void HandleOnObjectPreRemoved(const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle ObjectRow);
	void HandleOnObjectsReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap);

	UPROPERTY()
	TObjectPtr<UTypedElementDatabase> Database = nullptr;
	UPROPERTY()
	TObjectPtr<UTypedElementDatabaseCompatibility> DataStorageCompatibility = nullptr;
	UPROPERTY()
	TObjectPtr<UTypedElementMementoSystem> MementoSystem = nullptr;

	// Reverse lookup that holds all populated mementos for recently deleted objects
	// Entry removed when the memento is removed
	TMap<const void*, TypedElementRowHandle> OldObjectToMementoMap;
	
	TypedElementTableHandle MementoRowBaseTable;
	FDelegateHandle ReinstancingCallbackHandle;
	FDelegateHandle ObjectRemovedCallbackHandle;
};

USTRUCT()
struct FTypedElementsReinstanceableSourceObject : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	const void* Object;
};

USTRUCT()
struct FTypedElementsObjectReinstanceSourceColumnInitialized : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};