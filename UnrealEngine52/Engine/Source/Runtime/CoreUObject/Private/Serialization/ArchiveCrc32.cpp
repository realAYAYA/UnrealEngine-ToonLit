// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveCrc32.h"
#include "Serialization/ArchiveObjectCrc32.h"

#include "UObject/Object.h"


FArchiveCrc32::FArchiveCrc32(uint32 InCRC, UObject* InRootObject)
	: CRC(InCRC)
	, RootObject(InRootObject)
{
}

void FArchiveCrc32::Serialize(void* Data, int64 Num)
{
	CRC = FCrc::MemCrc32(Data, Num, CRC);
}

FArchive& FArchiveCrc32::operator<<(class FName& Name)
{
	CRC = FCrc::StrCrc32(*Name.ToString(), CRC);
	return *this;
}

FArchive& FArchiveCrc32::operator<<(class UObject*& Object)
{
	if (!RootObject || !Object || !Object->IsIn(RootObject))
	{
		CRC = FCrc::StrCrc32(*GetPathNameSafe(Object), CRC);
	}
	else
	{
		CRC = FArchiveObjectCrc32().Crc32(Object, RootObject, CRC);
	}

	return *this;
}
