// Copyright Epic Games, Inc. All Rights Reserved.

#include "InternalMinimalGameplayCueReplicationProxyNetSerializer.h"

#if UE_WITH_IRIS

#include "GameplayCueInterface.h"

void FMinimalGameplayCueReplicationProxyForNetSerializer::CopyReplicatedFieldsFrom(const FMinimalGameplayCueReplicationProxy& ReplicationProxy)
{
	Tags = ReplicationProxy.ReplicatedTags;
	Locations = ReplicationProxy.ReplicatedLocations;
}

void FMinimalGameplayCueReplicationProxyForNetSerializer::AssignReplicatedFieldsTo(FMinimalGameplayCueReplicationProxy& ReplicationProxy) const
{
	ReplicationProxy.ReplicatedTags = Tags;
	ReplicationProxy.ReplicatedLocations = Locations;
}

#endif
