// Copyright Epic Games, Inc. All Rights Reserved.

#include "IdentifierTable/ConcertTransportArchives.h"
#include "IdentifierTable/ConcertIdentifierTable.h"

enum class EConcertIdentifierSource : uint8
{
	/** Plain string value (no suffix) */
	PlainString,
	/** Hardcoded FName index value (see MAX_NETWORKED_HARDCODED_NAME) */
	HardcodedIndex,
	/** Local identifier table index value (see FConcertLocalIdentifierTable) */
	LocalIdentifierTableIndex,
};

FConcertIdentifierWriter::FConcertIdentifierWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, TArray<uint8>& InBytes, bool bIsPersistent)
	: FMemoryWriter(InBytes, bIsPersistent)
	, LocalIdentifierTable(InLocalIdentifierTable)
{
}

FArchive& FConcertIdentifierWriter::operator<<(FName& Name)
{
	auto SerializeConcertIdentifierSource = [this](EConcertIdentifierSource InSource)
	{
		Serialize(&InSource, sizeof(EConcertIdentifierSource));
	};

	auto SerializeIndexValue = [this](int32 InIndex)
	{
		check(InIndex >= 0);
		uint32 UnsignedIndex = (uint32)InIndex;
		SerializeIntPacked(UnsignedIndex);
	};

	const EName* Ename = Name.ToEName();
	int32 IdentifierTableIndex = INDEX_NONE;
	if (Ename && ShouldReplicateAsInteger(*Ename, Name))
	{
		SerializeConcertIdentifierSource(EConcertIdentifierSource::HardcodedIndex);
		SerializeIndexValue((int32)*Ename);
	}
	else if (LocalIdentifierTable)
	{
		SerializeConcertIdentifierSource(EConcertIdentifierSource::LocalIdentifierTableIndex);
		IdentifierTableIndex = LocalIdentifierTable->MapName(Name);
		SerializeIndexValue(IdentifierTableIndex);
	}
	else
	{
		SerializeConcertIdentifierSource(EConcertIdentifierSource::PlainString);
		FString PlainString = Name.GetPlainNameString();
		*this << PlainString;
	}
	
	int32 NameNumber = Name.GetNumber();
	SerializeIndexValue(NameNumber);

	return *this;
}

FArchive& FConcertIdentifierWriter::operator<<(FSoftObjectPath& AssetPtr)
{
	FName ObjPath = *AssetPtr.ToString();
	*this << ObjPath;
	return *this;
}

FString FConcertIdentifierWriter::GetArchiveName() const
{
	return TEXT("FConcertIdentifierWriter");
}


FConcertIdentifierReader::FConcertIdentifierReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, const TArray<uint8>& InBytes, bool bIsPersistent)
	: FMemoryReader(InBytes, bIsPersistent)
	, LocalIdentifierTable(InLocalIdentifierTable)
{
}

FArchive& FConcertIdentifierReader::operator<<(FName& Name)
{
	if (GetError())
	{
		return *this;
	}

	auto SerializeIndexValue = [this]() -> int32
	{
		uint32 UnsignedIndex = 0;
		SerializeIntPacked(UnsignedIndex);
		return (int32)UnsignedIndex;
	};

	EConcertIdentifierSource Source;
	Serialize(&Source, sizeof(EConcertIdentifierSource));

	switch (Source)
	{
	case EConcertIdentifierSource::PlainString:
		{
			FString PlainString;
			*this << PlainString;
			Name = FName(*PlainString, NAME_NO_NUMBER_INTERNAL, /*bSplitName*/false);
		}
		break;

	case EConcertIdentifierSource::HardcodedIndex:
		{
			const int32 HardcodedIndex = SerializeIndexValue();
			Name = EName(HardcodedIndex);
		}
		break;

	case EConcertIdentifierSource::LocalIdentifierTableIndex:
		{
			const int32 IdentifierTableIndex = SerializeIndexValue();
			if (!LocalIdentifierTable || !LocalIdentifierTable->UnmapName(IdentifierTableIndex, Name))
			{
				SetError();
				return *this;
			}
		}
		break;

	default:
		checkf(false, TEXT("Unknown EConcertIdentifierSource!"));
		break;
	}

	const int32 NameNumber = SerializeIndexValue();
	Name.SetNumber(NameNumber);

	return *this;
}

FArchive& FConcertIdentifierReader::operator<<(FSoftObjectPath& AssetPtr)
{
	FName ObjPath = *AssetPtr.ToString();
	*this << ObjPath;
	return *this;
}

FString FConcertIdentifierReader::GetArchiveName() const
{
	return TEXT("FConcertIdentifierReader");
}

