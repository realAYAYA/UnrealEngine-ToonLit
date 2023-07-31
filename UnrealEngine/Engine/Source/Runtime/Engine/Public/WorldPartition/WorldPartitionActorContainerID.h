// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Hash/CityHash.h"
#include "WorldPartitionActorContainerID.generated.h"

USTRUCT()
struct FActorContainerID
{
	GENERATED_USTRUCT_BODY()

	FActorContainerID()
	: ID(0)
	{}

	FActorContainerID(const FActorContainerID& InOther)
	: ID(InOther.ID)
	{}

	FActorContainerID(const FActorContainerID& InParent, FGuid InActorGuid)
	: ID(CityHash64WithSeed((const char*)&InActorGuid, sizeof(InActorGuid), InParent.ID))
	{}

	void operator=(const FActorContainerID& InOther)
	{
		ID = InOther.ID;
	}

	bool operator==(const FActorContainerID& InOther) const
	{
		return ID == InOther.ID;
	}

	bool operator!=(const FActorContainerID& InOther) const
	{
		return ID != InOther.ID;
	}

	bool IsMainContainer() const
	{
		return !ID;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%016llx"), ID);
	}

	friend FORCEINLINE uint32 GetTypeHash(const FActorContainerID& InContainerID)
	{
		return GetTypeHash(InContainerID.ID);
	}

	static FActorContainerID GetMainContainerID()
	{
		return FActorContainerID();
	}

	UPROPERTY()
	uint64 ID;
};
