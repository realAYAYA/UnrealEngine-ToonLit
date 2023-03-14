// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/NetworkObjectList.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Serialization/Archive.h"
#include "Net/NetworkGranularMemoryLogging.h"

namespace UE::Net::Private
{
	bool bTrackDormantObjectsByLevel = false;
	static FAutoConsoleVariableRef CVarNetTrackDormantObjectsByLevel(
		TEXT("net.TrackDormantObjectsByLevel"),
		bTrackDormantObjectsByLevel,
		TEXT("When true, network object list will maintain a set of dormant actors per connnection per level."),
		ECVF_Default);
}

void FNetworkObjectList::AddInitialObjects(UWorld* const World, UNetDriver* NetDriver)
{
	if (World == nullptr || NetDriver == nullptr)
	{
		return;
	}

#if UE_WITH_IRIS
	if (NetDriver->GetReplicationSystem())
	{
		return;
	}
#endif // UE_WITH_IRIS


	for (FActorIterator Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (IsValid(Actor) && ULevel::IsNetActor(Actor) && !UNetDriver::IsDormInitialStartupActor(Actor))
		{
			FindOrAdd(Actor, NetDriver);
		}
	}
}

TSharedPtr<FNetworkObjectInfo> FNetworkObjectList::Find(AActor* const Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (TSharedPtr<FNetworkObjectInfo>* InfoPtr = AllNetworkObjects.Find(Actor))
	{
		return *InfoPtr;
	}

	return TSharedPtr<FNetworkObjectInfo>();
}

TSharedPtr<FNetworkObjectInfo>* FNetworkObjectList::FindOrAdd(AActor* const Actor, UNetDriver* NetDriver, bool* OutWasAdded)
{
	if (!IsValid(Actor) ||

		// This implies the actor was added either added sometime during UWorld::DestroyActor,
		// or was potentially previously destroyed (and its Index points to a diferent, non-PendingKill object).
		!ensureAlwaysMsgf(!Actor->IsActorBeingDestroyed(), TEXT("Attempting to add an actor that's being destroyed to the NetworkObjectList Actor=%s NetDriverName=%s"), *Actor->GetPathName(), NetDriver ? *NetDriver->NetDriverName.ToString() : TEXT("None")))
	{
		return nullptr;
	}

#if UE_WITH_IRIS
	if (NetDriver && NetDriver->GetReplicationSystem())
	{
		return nullptr;
	}
#endif // UE_WITH_IRIS

	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfo = AllNetworkObjects.Find(Actor);
	
	if (NetworkObjectInfo == nullptr)
	{
		// We do a name check here so we don't add an actor to a network list that it shouldn't belong to
		if (NetDriver && NetDriver->ShouldReplicateActor(Actor))
		{
			NetworkObjectInfo = &AllNetworkObjects[AllNetworkObjects.Emplace(new FNetworkObjectInfo(Actor))];
			ActiveNetworkObjects.Add(*NetworkObjectInfo);

			UE_LOG(LogNetDormancy, VeryVerbose, TEXT("FNetworkObjectList::Add: Adding actor. Actor: %s, Total: %i, Active: %i, NetDriverName: %s"), *Actor->GetName(), AllNetworkObjects.Num(), ActiveNetworkObjects.Num(), *NetDriver->NetDriverName.ToString());

			if (OutWasAdded)
			{
				*OutWasAdded = true;
			}
		}
	}
	else
	{
		UE_LOG(LogNetDormancy, VeryVerbose, TEXT("FNetworkObjectList::Add: Already contained. Actor: %s, Total: %i, Active: %i, NetDriverName: %s"), *Actor->GetName(), AllNetworkObjects.Num(), ActiveNetworkObjects.Num(), NetDriver ? *NetDriver->NetDriverName.ToString() : TEXT("None"));
		if (OutWasAdded)
		{
			*OutWasAdded = false;
		}
	}
	
	check((ActiveNetworkObjects.Num() + ObjectsDormantOnAllConnections.Num()) == AllNetworkObjects.Num());

	return NetworkObjectInfo;
}

void FNetworkObjectList::Remove(AActor* const Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfoPtr = AllNetworkObjects.Find(Actor);

	if (NetworkObjectInfoPtr == nullptr)
	{
		// Sanity check that we're not on the other lists either
		check(!ActiveNetworkObjects.Contains(Actor));
		check(!ObjectsDormantOnAllConnections.Contains(Actor));
		check((ActiveNetworkObjects.Num() + ObjectsDormantOnAllConnections.Num()) == AllNetworkObjects.Num());
		return;
	}

	const FName PackageName = Actor->GetLevel()->GetPackage()->GetFName();

	FNetworkObjectInfo* NetworkObjectInfo = NetworkObjectInfoPtr->Get();

	// Lower the dormant object count for each connection this object is dormant on
	for (auto ConnectionIt = NetworkObjectInfo->DormantConnections.CreateIterator(); ConnectionIt; ++ConnectionIt)
	{
		UNetConnection* Connection = (*ConnectionIt).Get();

		if (Connection == nullptr || Connection->GetConnectionState() == USOCK_Closed)
		{
			if (UE::Net::Private::bTrackDormantObjectsByLevel)
			{
				DormantObjectsPerConnection.Remove(Connection);
			}

			ConnectionIt.RemoveCurrent();
			continue;
		}

		int32& NumDormantObjectsPerConnectionRef = NumDormantObjectsPerConnection.FindOrAdd(Connection);
		check(NumDormantObjectsPerConnectionRef > 0);
		NumDormantObjectsPerConnectionRef--;

		if (UE::Net::Private::bTrackDormantObjectsByLevel)
		{
			if (TMap<FName, FNetworkObjectSet>* DormantObjectsByLevel = DormantObjectsPerConnection.Find(Connection))
			{
				if (FNetworkObjectSet* DormantObjects = DormantObjectsByLevel->Find(PackageName))
				{
					DormantObjects->Remove(Actor);
					
					if (DormantObjects->Num() == 0)
					{
						DormantObjectsByLevel->Remove(PackageName);
					}
				}
			}
		}
	}

	// Remove this object from all lists
	AllNetworkObjects.Remove(Actor);
	ActiveNetworkObjects.Remove(Actor);
	ObjectsDormantOnAllConnections.Remove(Actor);

	if (UE::Net::Private::bTrackDormantObjectsByLevel)
	{
		if (FNetworkObjectSet* FullyDormant = FullyDormantObjectsByLevel.Find(PackageName))
		{
			FullyDormant->Remove(Actor);

			if (FullyDormant->Num() == 0)
			{
				FullyDormantObjectsByLevel.Remove(PackageName);
			}
		}
	}

	check((ActiveNetworkObjects.Num() + ObjectsDormantOnAllConnections.Num()) == AllNetworkObjects.Num());
}

void FNetworkObjectList::MarkDormant(AActor* const Actor, UNetConnection* const Connection, const int32 NumConnections, UNetDriver* NetDriver)
{
	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfoPtr = FindOrAdd(Actor, NetDriver);

	if (NetworkObjectInfoPtr == nullptr)
	{
		return;		// Actor doesn't belong to this net driver name
	}

	const FName PackageName = Actor->GetLevel()->GetPackage()->GetFName();

	FNetworkObjectInfo* NetworkObjectInfo = NetworkObjectInfoPtr->Get();

	// Add the connection to the list of dormant connections (if it's not already on the list)
	if (!NetworkObjectInfo->DormantConnections.Contains(Connection))
	{
		check(ActiveNetworkObjects.Contains(Actor));

		NetworkObjectInfo->DormantConnections.Add(Connection);

		// Keep track of the number of dormant objects on each connection
		int32& NumDormantObjectsPerConnectionRef = NumDormantObjectsPerConnection.FindOrAdd(Connection);
		NumDormantObjectsPerConnectionRef++;

		if (UE::Net::Private::bTrackDormantObjectsByLevel)
		{
			// make sure the connection map exists
			TMap<FName, FNetworkObjectSet>& DormantObjectsByLevel = DormantObjectsPerConnection.FindOrAdd(Connection);

			// if not fully dormant
			if (NetworkObjectInfo->DormantConnections.Num() != NumConnections)
			{
				FNetworkObjectSet& DormantObjects = DormantObjectsByLevel.FindOrAdd(PackageName);
				DormantObjects.Add(*NetworkObjectInfoPtr);
			}
		}

		UE_LOG(LogNetDormancy, Log, TEXT("FNetworkObjectList::MarkDormant: Actor is now dormant. Actor: %s. NumDormant: %i, Connection: %s"), *Actor->GetName(), NumDormantObjectsPerConnectionRef, *Connection->GetName());
	}

	// Clean up DormantConnections list (remove possible GC'd connections)
	for (auto ConnectionIt = NetworkObjectInfo->DormantConnections.CreateIterator(); ConnectionIt; ++ConnectionIt)
	{
		if ((*ConnectionIt).Get() == nullptr || (*ConnectionIt).Get()->GetConnectionState() == USOCK_Closed)
		{
			if (UE::Net::Private::bTrackDormantObjectsByLevel)
			{
				DormantObjectsPerConnection.Remove((*ConnectionIt).Get());
			}

			ConnectionIt.RemoveCurrent();
		}
	}

	// At this point, after removing null references, we should never be over the connection count
	check(NetworkObjectInfo->DormantConnections.Num() <= NumConnections);

	// If the number of dormant connections now matches the number of actual connections, we can remove this object from the active list
	if (NetworkObjectInfo->DormantConnections.Num() == NumConnections)
	{
		ObjectsDormantOnAllConnections.Add(*NetworkObjectInfoPtr);

		if (UE::Net::Private::bTrackDormantObjectsByLevel)
		{
			FNetworkObjectSet& FullyDormant = FullyDormantObjectsByLevel.FindOrAdd(PackageName);
			FullyDormant.Add(*NetworkObjectInfoPtr);

			// Remove from connection object lists
			for (auto ConnectionIt = NetworkObjectInfo->DormantConnections.CreateIterator(); ConnectionIt; ++ConnectionIt)
			{
				if (TMap<FName, FNetworkObjectSet>* DormantObjectsByLevel = DormantObjectsPerConnection.Find((*ConnectionIt).Get()))
				{
					if (FNetworkObjectSet* DormantObjects = DormantObjectsByLevel->Find(PackageName))
					{
						const int32 NumRemoved = DormantObjects->Remove(Actor);
						checkf((NumRemoved > 0) || (Connection == (*ConnectionIt).Get()), TEXT("Actor not found in Connection->Level->Dormant map: %s"), *GetNameSafe(Actor));

						if (DormantObjects->Num() == 0)
						{
							DormantObjectsByLevel->Remove(PackageName);
						}
					}
				}
			}
		}

		ActiveNetworkObjects.Remove(Actor);

		UE_LOG(LogNetDormancy, Log, TEXT("FNetworkObjectList::MarkDormant: Actor is now dormant on all connections. Actor: %s. Total: %i, Active: %i, Connection: %s"), *Actor->GetName(), AllNetworkObjects.Num(), ActiveNetworkObjects.Num(), *Connection->GetName());
	}

	check((ActiveNetworkObjects.Num() + ObjectsDormantOnAllConnections.Num()) == AllNetworkObjects.Num());
}

bool FNetworkObjectList::MarkActiveInternal(const TSharedPtr<FNetworkObjectInfo>& ObjectInfo, UNetConnection* const Connection, UNetDriver* NetDriver)
{
	FNetworkObjectInfo* NetworkObjectInfo = ObjectInfo.Get();
	AActor* Actor = NetworkObjectInfo->Actor;

	const FName PackageName = Actor->GetLevel()->GetPackage()->GetFName();

	// Remove from the ObjectsDormantOnAllConnections if needed
	if (ObjectsDormantOnAllConnections.Remove(Actor) > 0)
	{
		// Put this object back on the active list
		ActiveNetworkObjects.Add(ObjectInfo);

		UE_LOG(LogNetDormancy, Log, TEXT("FNetworkObjectList::MarkDormant: Actor is no longer dormant on all connections. Actor: %s. Total: %i, Active: %i, Connection: %s"), *Actor->GetName(), AllNetworkObjects.Num(), ActiveNetworkObjects.Num(), *Connection->GetName());

		if (UE::Net::Private::bTrackDormantObjectsByLevel)
		{
			if (FNetworkObjectSet* FullyDormant = FullyDormantObjectsByLevel.Find(PackageName))
			{
				const int32 NumRemoved = FullyDormant->Remove(Actor);
				checkf(NumRemoved > 0, TEXT("Actor not found in full Level->Dormant map: %s"), *GetNameSafe(Actor));

				if (FullyDormant->Num() == 0)
				{
					FullyDormantObjectsByLevel.Remove(PackageName);
				}

				// add back into per connection maps
				for (auto ConnectionIt = NetworkObjectInfo->DormantConnections.CreateIterator(); ConnectionIt; ++ConnectionIt)
				{
					TMap<FName, FNetworkObjectSet>& DormantObjectsByLevel = DormantObjectsPerConnection.FindOrAdd((*ConnectionIt).Get());
					FNetworkObjectSet& DormantObjects = DormantObjectsByLevel.FindOrAdd(PackageName);
					DormantObjects.Add(ObjectInfo);
				}
			}
		}
	}

	check((ActiveNetworkObjects.Num() + ObjectsDormantOnAllConnections.Num()) == AllNetworkObjects.Num());

	// Remove connection from the dormant connection list
	if (NetworkObjectInfo->DormantConnections.Remove(Connection) > 0)
	{
		// Add the connection to the list of recently dormant connections
		NetworkObjectInfo->RecentlyDormantConnections.Add(Connection);

		int32& NumDormantObjectsPerConnectionRef = NumDormantObjectsPerConnection.FindOrAdd(Connection);
		check(NumDormantObjectsPerConnectionRef > 0);
		NumDormantObjectsPerConnectionRef--;

		if (UE::Net::Private::bTrackDormantObjectsByLevel)
		{
			if (TMap<FName, FNetworkObjectSet>* DormantObjectsByLevel = DormantObjectsPerConnection.Find(Connection))
			{
				if (FNetworkObjectSet* DormantObjects = DormantObjectsByLevel->Find(PackageName))
				{
					const int32 NumRemoved = DormantObjects->Remove(Actor);
					checkf(NumRemoved > 0, TEXT("Actor not found in Connection->Level->Dormant map: %s"), *GetNameSafe(Actor));

					if (DormantObjects->Num() == 0)
					{
						DormantObjectsByLevel->Remove(PackageName);
					}
				}
			}
		}

		UE_LOG(LogNetDormancy, Log, TEXT("FNetworkObjectList::MarkActive: Actor is no longer dormant. Actor: %s. NumDormant: %i, Connection: %s"), *Actor->GetName(), NumDormantObjectsPerConnectionRef, *Connection->GetName());
		return true;
	}

	return false;
}

bool FNetworkObjectList::MarkActive(AActor* const Actor, UNetConnection* const Connection, UNetDriver* NetDriver)
{
	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfoPtr = FindOrAdd(Actor, NetDriver);

	if (NetworkObjectInfoPtr == nullptr)
	{
		return false;		// Actor doesn't belong to this net driver name
	}

	return MarkActiveInternal(*NetworkObjectInfoPtr, Connection, NetDriver);
}

void FNetworkObjectList::MarkDirtyForReplay(AActor* const Actor)
{
	if (Actor)
	{
		if (TSharedPtr<FNetworkObjectInfo>* InfoPtr = AllNetworkObjects.Find(Actor))
		{
			if (FNetworkObjectInfo* ObjectInfo = InfoPtr->Get())
			{
				ObjectInfo->bDirtyForReplay = true;
			}
		}
	}
}

void FNetworkObjectList::ClearRecentlyDormantConnection(AActor* const Actor, UNetConnection* const Connection, UNetDriver* NetDriver)
{
	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfoPtr = FindOrAdd(Actor, NetDriver);

	if (NetworkObjectInfoPtr == nullptr)
	{
		return;		// Actor doesn't belong to this net driver name
	}

	FNetworkObjectInfo* NetworkObjectInfo = NetworkObjectInfoPtr->Get();

	NetworkObjectInfo->RecentlyDormantConnections.Remove(Connection);
}

void FNetworkObjectList::HandleConnectionAdded()
{
	// When a new connection is added, we must add all objects back to the active list so the new connection will process it
	// Once the objects is dormant on that connection, it will then be removed from the active list again
	for (auto It = ObjectsDormantOnAllConnections.CreateIterator(); It; ++It)
	{
		ActiveNetworkObjects.Add(*It);
	}

	ObjectsDormantOnAllConnections.Empty();
	FullyDormantObjectsByLevel.Empty();
}

void FNetworkObjectList::ResetDormancyState()
{
	// Reset all state related to dormancy, and move all objects back on to the active list
	ObjectsDormantOnAllConnections.Empty();
	FullyDormantObjectsByLevel.Empty();

	ActiveNetworkObjects = AllNetworkObjects;

	for (auto It = AllNetworkObjects.CreateIterator(); It; ++It)
	{
		FNetworkObjectInfo* NetworkObjectInfo = ( *It ).Get();

		NetworkObjectInfo->DormantConnections.Empty();
		NetworkObjectInfo->RecentlyDormantConnections.Empty();
	}

	NumDormantObjectsPerConnection.Empty();
	DormantObjectsPerConnection.Empty();
}

int32 FNetworkObjectList::GetNumDormantActorsForConnection(UNetConnection* const Connection) const
{
	const int32 *Count = NumDormantObjectsPerConnection.Find( Connection );

	return (Count != nullptr) ? *Count : 0;
}

void FNetworkObjectList::ForceActorRelevantNextUpdate(AActor* const Actor, UNetDriver* NetDriver)
{
	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfoPtr = FindOrAdd(Actor, NetDriver);

	if (NetworkObjectInfoPtr == nullptr)
	{
		return;		// Actor doesn't belong to this net driver name
	}

	FNetworkObjectInfo* NetworkObjectInfo = NetworkObjectInfoPtr->Get();

	NetworkObjectInfo->ForceRelevantFrame = NetDriver->ReplicationFrame + 1;
}

void FNetworkObjectList::Reset()
{
	// Reset all state
	AllNetworkObjects.Empty();
	ActiveNetworkObjects.Empty();
	ObjectsDormantOnAllConnections.Empty();
	NumDormantObjectsPerConnection.Empty();
	FullyDormantObjectsByLevel.Empty();
	DormantObjectsPerConnection.Empty();
}

void FNetworkObjectInfo::CountBytes(FArchive& Ar) const
{
	GRANULAR_NETWORK_MEMORY_TRACKING_INIT(Ar, "FNetworkObjectInfo::CountBytes");

	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("DormantConnections", DormantConnections.CountBytes(Ar));
	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("RecentlyDormantConnections", RecentlyDormantConnections.CountBytes(Ar));
}

void FNetworkObjectList::CountBytes(FArchive& Ar) const
{
	GRANULAR_NETWORK_MEMORY_TRACKING_INIT(Ar, "FNetworkObjectList::CountBytes");

	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("ActiveNetworkObjects", ActiveNetworkObjects.CountBytes(Ar));
	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("ObjectsDormantOnAllConnections", ObjectsDormantOnAllConnections.CountBytes(Ar));
	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("NumDormantObjectsPerConnection", NumDormantObjectsPerConnection.CountBytes(Ar));

	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("FullyDormantObjectsByLevel",
		FullyDormantObjectsByLevel.CountBytes(Ar);
		for (const TPair<FName, FNetworkObjectSet>& LevelPair : FullyDormantObjectsByLevel)
		{
			LevelPair.Value.CountBytes(Ar);
		}
	);
	
	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("DormantObjectsPerConnection",
		DormantObjectsPerConnection.CountBytes(Ar);
		for (const TPair<TObjectKey<UNetConnection>, TMap<FName, FNetworkObjectSet>>& ConnectionPair : DormantObjectsPerConnection)
		{
			ConnectionPair.Value.CountBytes(Ar);

			for (const TPair<FName, FNetworkObjectSet>& LevelPair : ConnectionPair.Value)
			{
				LevelPair.Value.CountBytes(Ar);
			}
		}
	);

	// ObjectsDormantOnAllConnections and ActiveNetworkObjects are both sub sets of AllNetworkObjects
	// and only have pointers back to the data there.
	// So, to avoid double (or triple) counting, only explicit count the elements from AllNetworkObjects.
	GRANULAR_NETWORK_MEMORY_TRACKING_TRACK("AllNetworkObjects",
		AllNetworkObjects.CountBytes(Ar);
		for (const TSharedPtr<FNetworkObjectInfo>& SharedInfo : AllNetworkObjects)
		{
			if (FNetworkObjectInfo const* const Info = SharedInfo.Get())
			{
				Ar.CountBytes(sizeof(FNetworkObjectInfo), sizeof(FNetworkObjectInfo));
				Info->CountBytes(Ar);
			}
		}
	);
}

void FNetworkObjectList::FlushDormantActors(UNetConnection* const Connection, const FName& PackageName)
{
	FNetworkObjectSet DormantActors;

	// fully dormant actors
	if (const FNetworkObjectSet* FullyDormantObjects = FullyDormantObjectsByLevel.Find(PackageName))
	{
		DormantActors.Append(*FullyDormantObjects);
	}

	// connection specific dormant actors
	if (const TMap<FName, FNetworkObjectSet>* DormantObjectsByLevel = DormantObjectsPerConnection.Find(Connection))
	{
		if (const FNetworkObjectSet* DormantObjects = DormantObjectsByLevel->Find(PackageName))
		{
			DormantActors.Append(*DormantObjects);
		}
	}

	for (const TSharedPtr<FNetworkObjectInfo>& ActorInfo : DormantActors)
	{
		MarkActiveInternal(ActorInfo, Connection, Connection->Driver);
	}
}