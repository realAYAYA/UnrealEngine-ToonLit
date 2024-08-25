// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"

class FTypedElementDatabaseIndexTable final
{
public:
	TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const;
	void IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row);
	void ReindexRow(
		TypedElementDataStorage::IndexHash OriginalIndex,
		TypedElementDataStorage::IndexHash NewIndex,
		TypedElementDataStorage::RowHandle Row);
	void RemoveIndex(TypedElementDataStorage::IndexHash Index);
	void RemoveRow(TypedElementDataStorage::RowHandle Row);

private:
	TMap<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle> IndexLookupMap;
	TMultiMap<TypedElementDataStorage::RowHandle, TypedElementDataStorage::IndexHash> ReverseIndexLookupMap;
};
