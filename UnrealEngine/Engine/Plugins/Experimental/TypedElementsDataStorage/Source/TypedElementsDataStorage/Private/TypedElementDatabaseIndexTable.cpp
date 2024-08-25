// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseIndexTable.h"

TypedElementDataStorage::RowHandle FTypedElementDatabaseIndexTable::FindIndexedRow(TypedElementDataStorage::IndexHash Index) const
{
	const TypedElementDataStorage::RowHandle* Result = IndexLookupMap.Find(Index);
	return Result ? *Result : TypedElementDataStorage::InvalidRowHandle;
}

void FTypedElementDatabaseIndexTable::IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
{
	IndexLookupMap.Add(Index, Row);
	ReverseIndexLookupMap.Add(Row, Index);
}

void FTypedElementDatabaseIndexTable::ReindexRow(TypedElementDataStorage::IndexHash OriginalIndex, 
	TypedElementDataStorage::IndexHash NewIndex, TypedElementDataStorage::RowHandle Row)
{
	RemoveIndex(OriginalIndex);
	IndexRow(NewIndex, Row);
}

void FTypedElementDatabaseIndexTable::RemoveIndex(TypedElementDataStorage::IndexHash Index)
{
	using namespace TypedElementDataStorage;

	if (const RowHandle* Row = IndexLookupMap.Find(Index))
	{
		IndexLookupMap.Remove(Index);
		ReverseIndexLookupMap.Remove(*Row, Index);
	}
}

void FTypedElementDatabaseIndexTable::RemoveRow(TypedElementDataStorage::RowHandle Row)
{
	using namespace TypedElementDataStorage;
	
	if (TMultiMap<RowHandle, IndexHash>::TKeyIterator It = ReverseIndexLookupMap.CreateKeyIterator(Row); It)
	{
		do
		{
			IndexLookupMap.Remove(It.Value());
			++It;
		} while (It);
		ReverseIndexLookupMap.Remove(Row);
	}
}