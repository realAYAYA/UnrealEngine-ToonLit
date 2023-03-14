// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

class FConcertLocalIdentifierTable;

/** Archive for writing identifiers (currently names) in a way that avoids duplication by caching them against their internal key, which can then be mapped over the network */
class CONCERTTRANSPORT_API FConcertIdentifierWriter : public FMemoryWriter
{
public:
	FConcertIdentifierWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, TArray<uint8>& InBytes, bool bIsPersistent = false);

	using FMemoryWriter::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

private:
	FConcertLocalIdentifierTable* LocalIdentifierTable;
};

/** Archive for reading identifiers (currently names) in a way that avoids duplication by caching them against their internal key, which can then be mapped over the network  */
class CONCERTTRANSPORT_API FConcertIdentifierReader : public FMemoryReader
{
public:
	FConcertIdentifierReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, const TArray<uint8>& InBytes, bool bIsPersistent = false);

	using FMemoryReader::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(FName& Name) override;
	FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

private:
	const FConcertLocalIdentifierTable* LocalIdentifierTable;
};
