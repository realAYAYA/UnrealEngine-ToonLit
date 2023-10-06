// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hashing/ActorSnapshotHash.h"

#include "SnapshotCustomVersion.h"

bool FActorSnapshotHash::Serialize(FArchive& Archive)
{
	using namespace UE::LevelSnapshots;
	Archive.UsingCustomVersion(FSnapshotCustomVersion::GUID);

	if (Archive.CustomVer(FSnapshotCustomVersion::GUID) >= FSnapshotCustomVersion::FixActorHashSerialize)
	{
		Archive << MicroSecondsForCrc;
		Archive << MicroSecondsForMD5;
		Archive << Crc32DataLength;
		Archive << MD5DataLength;
		Archive << Crc32;
		Archive << MD5;
		return true;
	}

	// Use tagged property serialization
	return false;
}