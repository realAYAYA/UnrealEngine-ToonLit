// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ObjectReplicationMap.h"
#include "UObject/Object.h"
#include "ReplicationStreamObject.generated.h"

/** UObject wrapper to allow transactions on FConcertObjectReplicationMap. */
UCLASS()
class UReplicationStreamObject : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY()
	FConcertObjectReplicationMap ReplicationMap;
};