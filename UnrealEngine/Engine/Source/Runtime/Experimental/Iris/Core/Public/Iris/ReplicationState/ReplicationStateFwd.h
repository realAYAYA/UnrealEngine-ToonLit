// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Net
{

namespace Private
{
	struct FReplicationStateHeaderAccessor;
}

/** 
 * A ReplicationState always contains a ReplicationStateHeader which we use to bind replication states for dirty tracking
 */
struct FReplicationStateHeader
{
	inline FReplicationStateHeader() : ReplicationIndex(0), ReplicationSystemId(0), bInitStateIsDirty(0), ReplicationFlags(0) {};

	/** Returns true if the state is bound to the dirty tracking system */
	bool IsBound() const { return ReplicationIndex != 0U; }

private:
	friend Private::FReplicationStateHeaderAccessor;

	// All replication states that are bound by a instance protocol as assigned an index used for dirty state tracking
	uint32 ReplicationIndex : 24;
	// In order to support PIE we need to be able to manage multiple replication systems
	uint32 ReplicationSystemId : 4;
	// Init state doesn't use changemasks, instead we have a reserved bit here
	uint32 bInitStateIsDirty : 1;
	// Unused so far
	uint32 ReplicationFlags : 3;
};

static_assert(sizeof(FReplicationStateHeader) == sizeof(uint32) && alignof(FReplicationStateHeader) == alignof(uint32), "FReplicationStateHeader must currently have the same size and alignment as uint32");

namespace Private
{

/** 
 * Internal access, should only be used by internal code
 */
struct FReplicationStateHeaderAccessor
{
	static uint32 GetReplicationIndex(const FReplicationStateHeader& Header) { return Header.ReplicationIndex; }
	static uint32 GetReplicationSystemId(const FReplicationStateHeader& Header) { return Header.ReplicationSystemId; }
	static bool GetIsInitStateDirty(const FReplicationStateHeader& Header) { return Header.bInitStateIsDirty; }
	static uint32 GetReplicationFlags(const FReplicationStateHeader& Header) { return Header.ReplicationFlags; }

	static void MarkInitStateDirty(FReplicationStateHeader& Header) { Header.bInitStateIsDirty = true; }
	static void ClearInitStateIsDirty(FReplicationStateHeader& Header) { Header.bInitStateIsDirty = false; }
	static void SetReplicationIndex(FReplicationStateHeader& Header, uint32 Index, uint32 ReplicationSystemId) { Header.ReplicationIndex = Index; Header.ReplicationSystemId = ReplicationSystemId; }
};

}

}
