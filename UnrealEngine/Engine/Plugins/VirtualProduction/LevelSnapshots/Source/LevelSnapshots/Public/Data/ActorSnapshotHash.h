// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/SecureHash.h"
#include "ActorSnapshotHash.generated.h"

/** Every actor is hashed when it is saved so we can quickly check whether an actor has changed. */
USTRUCT()
struct LEVELSNAPSHOTS_API FActorSnapshotHash
{
	GENERATED_BODY()

	/**
	 * How many micro seconds it took to compute the actor CRC32 during saving. Used when loading actors.
	 * If the hash time is excessively high, it is more efficient simply to load the actor. Configured in project settings.
	 */
	UPROPERTY()
	double MicroSecondsForCrc = -1.0;

	/** How many micro seconds it took to compute the MD5 hash. */
	UPROPERTY()
	double MicroSecondsForMD5 = -1.0;
	
	/** How many bytes of data were in the data were used for CRC32. Used to avoid computing hash. */
	UPROPERTY()
	int32 Crc32DataLength {};
	
	/** How many bytes of data were in the data were used for MD5. Used to avoid computing hash. */
	UPROPERTY()
	int32 MD5DataLength {};

	/** Crc32 hash of actor when it was snapshot. Used to check for changes without loading actor. */
	UPROPERTY()
	uint32 Crc32 {};

	FMD5Hash MD5;
	
	bool HasCrc32() const { return MicroSecondsForCrc > 0.0; }
	bool HasMD5() const { return MicroSecondsForMD5 > 0.0; }

	bool Serialize(FArchive& Archive)
	{
		Archive << MicroSecondsForCrc;
		Archive << MicroSecondsForMD5;
		Archive << Crc32DataLength;
		Archive << MD5DataLength;
		Archive << Crc32;
		Archive << MD5;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FActorSnapshotHash> : public TStructOpsTypeTraitsBase2<FActorSnapshotHash>
{
	enum 
	{ 
		WithSerialize = true
	};
};