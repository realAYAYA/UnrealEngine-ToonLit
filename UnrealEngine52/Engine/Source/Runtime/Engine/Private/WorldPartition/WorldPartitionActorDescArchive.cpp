// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDescArchive.h"

FActorDescArchive::FActorDescArchive(FArchive& InArchive)
	: FArchiveProxy(InArchive)
{
	check(InArchive.IsPersistent());		

	SetIsPersistent(true);
	SetIsLoading(InArchive.IsLoading());		
}
#endif