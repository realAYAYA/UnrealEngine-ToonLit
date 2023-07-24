// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Net/Core/NetHandle/NetHandle.h"

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
	FReplicationStateHeader() : NetHandleId(0), bInitStateIsDirty(0), bStateIsDirty(0) {}

	/** Returns true if the state is bound to the dirty tracking system */
	bool IsBound() const { return NetHandleId != 0; }

private:
	friend Private::FReplicationStateHeaderAccessor;

	// All replication states that are bound by an instance protocol is assigned a NetHandle for dirty state tracking
	uint32 NetHandleId : 30;
	// Init state doesn't use changemasks, instead we have a reserved bit here
	uint32 bInitStateIsDirty : 1;
	// Track whether any state is dirty.
	uint32 bStateIsDirty : 1;
};

static_assert(sizeof(FReplicationStateHeader) == sizeof(uint32) && alignof(FReplicationStateHeader) == alignof(uint32), "FReplicationStateHeader must currently have the same size and alignment as uint32");

namespace Private
{

/** 
 * Internal access, should only be used by internal code
 */
struct FReplicationStateHeaderAccessor
{
	static uint32 GetNetHandleId(const FReplicationStateHeader& Header) { return Header.NetHandleId; }
	static bool GetIsInitStateDirty(const FReplicationStateHeader& Header) { return Header.bInitStateIsDirty; }
	static bool GetIsStateDirty(const FReplicationStateHeader& Header) { return Header.bStateIsDirty; }

	static void MarkInitStateDirty(FReplicationStateHeader& Header) { Header.bInitStateIsDirty = true; }
	static void MarkStateDirty(FReplicationStateHeader& Header) { Header.bStateIsDirty = true; }

	/** Clears both state and init state dirtiness. */
	static void ClearAllStateIsDirty(FReplicationStateHeader& Header)
	{ 
		Header.bInitStateIsDirty = false;
		Header.bStateIsDirty = false;
	}

	static void SetNetHandleId(FReplicationStateHeader& Header, FNetHandle NetHandle) { Header.NetHandleId = NetHandle.GetId(); }
};

}

}
