// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "UObject/UObjectGlobals.h"

#include "TypedElementMementoSystem.generated.h"

class ITypedElementDataStorageInterface;
class IConsoleVariable;
struct FTypedElementDatabaseCompatibilityObjectTypeInfo;
class UTypedElementDatabase;

UCLASS()
class UTypedElementMementoSystem : public UObject
{
	GENERATED_BODY()
public:

	void Initialize(UTypedElementDatabase& DataStorage);
	void Deinitialize();

	TypedElementRowHandle CreateMemento(ITypedElementDataStorageInterface* DataStorage);

private:
	void RegisterQueries(UTypedElementDatabase& DataStorage) const;
	void RegisterTables(UTypedElementDatabase& DataStorage);
	
	TypedElementTableHandle MementoRowBaseTable;
};
