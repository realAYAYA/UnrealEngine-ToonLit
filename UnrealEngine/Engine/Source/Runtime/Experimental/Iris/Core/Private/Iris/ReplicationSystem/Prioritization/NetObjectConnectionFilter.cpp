// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectConnectionFilter.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/Core/IrisProfiler.h"

void UNetObjectConnectionFilter::SetReplicateToConnection(UE::Net::FNetRefHandle RefHandle, uint32 ConnectionId, UE::Net::ENetFilterStatus FilterStatus)
{
	if (!ensure(ConnectionId < static_cast<uint32>(PerConnectionInfos.Num())))
	{
		return;
	}

	const UE::Net::Private::FInternalNetRefIndex ObjectIndex = NetRefHandleManager->GetInternalIndex(RefHandle);
	if (ObjectIndex == UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	if (const FFilteringInfo* FilteringInfo = static_cast<const FFilteringInfo*>(GetFilteringInfo(ObjectIndex)))
	{
		const uint32 LocalIndex = FilteringInfo->GetLocalObjectIndex();
		FPerConnectionInfo& PerConnectionInfo = PerConnectionInfos[ConnectionId];
		PerConnectionInfo.ReplicationEnabledObjects.SetBitValue(LocalIndex, FilterStatus == UE::Net::ENetFilterStatus::Allow);
	}
}

void UNetObjectConnectionFilter::OnInit(FNetObjectFilterInitParams& Params)
{
	NetRefHandleManager = &Params.ReplicationSystem->GetReplicationSystemInternal()->GetNetRefHandleManager();

	Config = TStrongObjectPtr<UNetObjectConnectionFilterConfig>(CastChecked<UNetObjectConnectionFilterConfig>(Params.Config));

	const uint16 MaxLocalObjectCount = static_cast<uint16>(FPlatformMath::Min<uint32>(Params.MaxObjectCount, Config->MaxObjectCount));
	UsedLocalInfoIndices.Init(MaxLocalObjectCount);
	LocalToNetRefIndex.SetNumZeroed(MaxLocalObjectCount);

	PerConnectionInfos.SetNum(Params.MaxConnectionCount + 1);
}

void UNetObjectConnectionFilter::AddConnection(uint32 ConnectionId)
{
	FPerConnectionInfo& ConnInfo = PerConnectionInfos[ConnectionId];
	ConnInfo.ReplicationEnabledObjects.Init(UsedLocalInfoIndices.GetNumBits());
}

void UNetObjectConnectionFilter::RemoveConnection(uint32 ConnectionId)
{
	// Free everything that was allocated for the connection.
	FPerConnectionInfo& ConnInfo = PerConnectionInfos[ConnectionId];
	ConnInfo = FPerConnectionInfo();
}

bool UNetObjectConnectionFilter::AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	const uint32 LocalIndex = UsedLocalInfoIndices.FindFirstZero();
	if (!ensureMsgf(LocalIndex != UE::Net::FNetBitArrayBase::InvalidIndex, TEXT("%hs. MaxObjectCount: %u. Config type %s."), "Too many objects added to NetObjectConnectionFilter. Object will not be handled by filter!", (Config.IsValid() ? Config->MaxObjectCount : uint16(0)), ToCStr(GetNameSafe(Config.Get()))))
	{
		return false;
	}

	UsedLocalInfoIndices.SetBit(LocalIndex);
	LocalToNetRefIndex[LocalIndex] = ObjectIndex;

	FFilteringInfo& Info = static_cast<FFilteringInfo&>(Params.OutInfo);
	Info.SetLocalObjectIndex(static_cast<uint16>(LocalIndex));

	return true;
}

void UNetObjectConnectionFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& InInfo)
{
	bObjectRemoved = true;

	const FFilteringInfo& Info = static_cast<const FFilteringInfo&>(InInfo);

	const uint16 LocalIndex = Info.GetLocalObjectIndex();
	UsedLocalInfoIndices.ClearBit(LocalIndex);

	// For good measure. It's not strictly needed.
	LocalToNetRefIndex[LocalIndex] = UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex;
}

void UNetObjectConnectionFilter::UpdateObjects(FNetObjectFilterUpdateParams&)
{
	// Intentionally left empty.
}

void UNetObjectConnectionFilter::PreFilter(FNetObjectPreFilteringParams& Params)
{
	if (!bObjectRemoved)
	{
		return;
	}

	bObjectRemoved = false;

	// Mask out no longer filtered objects to minimize looping during filtering.
	Params.ValidConnections.ForAllSetBits(
		[this](uint32 ConnectionId)
		{
			FPerConnectionInfo& Info = this->PerConnectionInfos[ConnectionId];
			Info.ReplicationEnabledObjects.Combine(this->UsedLocalInfoIndices, UE::Net::FNetBitArrayBase::AndOp);
		}
	);
}

void UNetObjectConnectionFilter::Filter(FNetObjectFilteringParams& Params)
{
	IRIS_PROFILER_SCOPE(UNetObjectConnectionFilter_Filter);

	UE::Net::FNetBitArrayView& AllowedObjects = Params.OutAllowedObjects;
	AllowedObjects.Reset();

	FPerConnectionInfo& ConnectionInfo = PerConnectionInfos[Params.ConnectionId];
	ConnectionInfo.ReplicationEnabledObjects.ForAllSetBits(
		[&AllowedObjects, &ToNetRefIndex = LocalToNetRefIndex](uint32 LocalObjectIndex)
		{
			const uint32 ObjectIndex = ToNetRefIndex[LocalObjectIndex];
			AllowedObjects.SetBit(ObjectIndex);
		}
	);
}
