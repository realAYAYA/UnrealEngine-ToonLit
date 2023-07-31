// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationState/ReplicationStateFwd.h"

namespace UE::Net::Private
{
	extern IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex);
}

namespace UE::Net
{
/**
 * Mark the a NetObject/Handle as dirty which means it requires to be copied when we update the Replication system
 */
inline void MarkNetObjectStateDirty(const FReplicationStateHeader& Header) { Private::MarkNetObjectStateDirty(Private::FReplicationStateHeaderAccessor::GetReplicationSystemId(Header), Private::FReplicationStateHeaderAccessor::GetReplicationIndex(Header)); }
}

namespace UE::Net::Private
{

/**
 * Get FReplicationStateHeader from a ReplicationState
 */
inline UE::Net::FReplicationStateHeader& GetReplicationStateHeader(void* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	// $IRIS: $TODO: Currently we do not store the offset to the internal replication state
	if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsNativeFastArrayReplicationState))
	{
		return *reinterpret_cast<UE::Net::FReplicationStateHeader*>(StateBuffer);
	}
	else
	{
		const SIZE_T ReplicationStateHeaderOffset = Descriptor->ChangeMasksExternalOffset + 8U;
		return *reinterpret_cast<UE::Net::FReplicationStateHeader*>(reinterpret_cast<uint8*>(StateBuffer) + ReplicationStateHeaderOffset);
	}
}

inline bool IsReplicationStateBound(void* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	return GetReplicationStateHeader(StateBuffer, Descriptor).IsBound();
}

/**
 * Get MemberChangeMask
 */
inline FNetBitArrayView GetMemberChangeMask(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	return FNetBitArrayView((FNetBitArrayView::StorageWordType*)(StateBuffer + Descriptor->GetChangeMaskOffset()), Descriptor->ChangeMaskBitCount, FNetBitArrayView::NoResetNoValidate);
}

/**
 * Get MemberConditionalChangeMask. Valid if state has lifetime conditionals.
 */
inline FNetBitArrayView GetMemberConditionalChangeMask(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	return FNetBitArrayView((FNetBitArrayView::StorageWordType*)(StateBuffer + Descriptor->GetConditionalChangeMaskOffset()), Descriptor->ChangeMaskBitCount, FNetBitArrayView::NoResetNoValidate);
}

/**
 * Mark specific member dirty, if this is the first bit marked as dirty in the local MemberChangeMask and the state is bound, mark owning object as dirty as well.
 * Note that all bits described by the ChangeMaskInfo will be dirtied by the call. 
 */
inline void MarkDirty(const UE::Net::FReplicationStateHeader& InternalState, FNetBitArrayView& MemberChangeMask, const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskInfo)
{
	// If this state is bound to a replicated object, notify the replication system that we have data to copy
	// Note that we only check the first bit of the changemask, as this is used to indicate whether the property is dirty or not
	// some properties might use additional bits but will only be treated as dirty if the parent bit is set.
	if (InternalState.IsBound() && !MemberChangeMask.GetBit(ChangeMaskInfo.BitOffset))
	{
		MarkNetObjectStateDirty(InternalState);
	}

	MemberChangeMask.SetBits(ChangeMaskInfo.BitOffset, ChangeMaskInfo.BitCount);
}

}
