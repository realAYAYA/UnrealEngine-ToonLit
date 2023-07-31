// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Net/Core/NetBitArray.h"

class UReplicationSystem;

// If set to 1 we include the size of our creation state so that we can skip that on clients
#define UE_NET_REPLICATION_SUPPORT_SKIP_INITIAL_STATE 0

namespace UE::Net::Private
{

struct FReplicationParameters
{
	uint32 MaxActiveReplicatedObjectCount;	
	uint32 PacketSendWindowSize;
	uint32 ConnectionId;
	UReplicationSystem* ReplicationSystem = nullptr;
	bool bAllowSendingAttachmentsToObjectsNotInScope = false;
	bool bAllowReceivingAttachmentsFromRemoteObjectsNotInScope = false;
	bool bAllowDelayingAttachmentsWithUnresolvedReferences = false;
	uint32 SmallObjectBitThreshold = 160U; // Number of bits remaining in a packet for us to consider trying to serialize a replicated object
	uint32 MaxFailedSmallObjectCount = 10U;	// Number of objects that we try to serialize after an initial stream overflow to fill up a packet, this can improve bandwidth usage but comes at a cpu cost
#if UE_NET_REPLICATION_SUPPORT_SKIP_INITIAL_STATE
	uint32 NumBitsUsedForInitialStateSize = 20U;
#endif
};

}
