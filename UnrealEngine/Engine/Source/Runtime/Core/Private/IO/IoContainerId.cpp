// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoContainerId.h"

#include "Hash/CityHash.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Char.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchiveSlots.h"

FIoContainerId FIoContainerId::FromName(const FName& Name)
{
	TCHAR NameStr[FName::StringBufferSize];
	uint32 NameLen = Name.ToString(NameStr);
	for (uint32 I = 0; I < NameLen; ++I)
	{
		NameStr[I] = TChar<TCHAR>::ToLower(NameStr[I]);
	}
	uint64 Hash = CityHash64(reinterpret_cast<const char*>(NameStr), NameLen * sizeof(TCHAR));
	checkf(Hash != InvalidId, TEXT("Container name hash collision \"%s\" and InvalidId"), NameStr);
	return FIoContainerId(Hash);
}

FArchive& operator<<(FArchive& Ar, FIoContainerId& ContainerId)
{
	Ar << ContainerId.Id;

	return Ar;
}

void operator<<(FStructuredArchiveSlot Slot, FIoContainerId& Value)
{
	Slot << Value.Id;
}
