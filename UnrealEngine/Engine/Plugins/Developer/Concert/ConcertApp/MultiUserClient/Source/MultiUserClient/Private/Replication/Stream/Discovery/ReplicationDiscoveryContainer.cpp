// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationDiscoveryContainer.h"

namespace UE::MultiUserClient
{
	void FReplicationDiscoveryContainer::AddDiscoverer(TSharedRef<IReplicationDiscoverer> Discoverer)
	{
		Discoverers.AddUnique(MoveTemp(Discoverer));
	}

	void FReplicationDiscoveryContainer::RemoveDiscoverer(const TSharedRef<IReplicationDiscoverer>& Discoverer)
	{
		Discoverers.RemoveSingle(Discoverer);
	}

	void FReplicationDiscoveryContainer::DiscoverReplicationSettings(const FReplicationDiscoveryParams& Params)
	{
		// Iterate in reverse order so we ignore any newly added ones
		for (int32 Index = Discoverers.Num() - 1; Discoverers.IsValidIndex(Index); --Index)
		{
			const TSharedRef<IReplicationDiscoverer>& Discoverer = Discoverers[Index];
			Discoverer->DiscoverReplicationSettings(Params);
			// if something was removed before i, we will visit them twice... which does not matter because adding properties is a no-op if they were already added.
		}
	}
}
