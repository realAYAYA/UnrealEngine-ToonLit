// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertIdentifierTableData.h"

/** Key functions that compare FName instances by only their display string (to perform a case-sensitive plain string comparison) */
template <typename ValueType>
struct TConcertIdentifierTable_CaseSensitivePlainNameKeyFuncs : BaseKeyFuncs<ValueType, FName, /*bInAllowDuplicateKeys*/false>
{
	static FORCEINLINE const FName& GetSetKey(const TPair<FName, ValueType>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FName& A, const FName& B)
	{
		return A.GetDisplayIndex() == B.GetDisplayIndex();
	}
	static FORCEINLINE uint32 GetKeyHash(const FName& Key)
	{
		return GetTypeHash(Key.GetDisplayIndex());
	}
};

/** Cache of identifiers (currently names) that have been serialized locally and should be sent along with the serialized data */
class CONCERTTRANSPORT_API FConcertLocalIdentifierTable
{
public:
	FConcertLocalIdentifierTable() = default;
	explicit FConcertLocalIdentifierTable(const FConcertLocalIdentifierState& InState);

	/**
	 * Map the given name to its identifier index.
	 */
	int32 MapName(const FName& InName);

	/**
	 * Unmap the given name from its identifier index (or NAME_None).
	 * @return true if the name was unmapped, false if the name wasn't mapped (OutName will be set to NAME_None).
	 */
	bool UnmapName(const int32 InIndex, FName& OutName) const;

	/**
	 * Is the given identifier index mapped?
	 */
	bool HasName(const int32 InIndex) const;

	/**
	 * Is the given name mapped?
	 */
	bool HasName(const FName& InName, int32* OutIndexPtr = nullptr) const;

	/**
	 * Set the state of this identifier table.
	 */
	void SetState(const FConcertLocalIdentifierState& InState);

	/**
	 * Get the state of this identifier table.
	 */
	void GetState(FConcertLocalIdentifierState& OutState) const;

private:
	/** Array of locally mapped names */
	TArray<FName> MappedNames;
	/** Map of known names to their index in MappedNames (name -> index) */
	TMap<FName, int32, FDefaultSetAllocator, TConcertIdentifierTable_CaseSensitivePlainNameKeyFuncs<int32>> NameToMappedIndex;
};
