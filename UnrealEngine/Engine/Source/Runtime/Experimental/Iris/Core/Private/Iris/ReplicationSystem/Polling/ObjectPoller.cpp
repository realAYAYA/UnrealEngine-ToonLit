// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectPoller.h"

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"

#include "Iris/Core/IrisProfiler.h"
#include "Iris/Stats/NetStatsContext.h"

#include "Net/Core/Trace/NetDebugName.h"

namespace UE::Net::Private
{

FObjectPoller::FObjectPoller(const FInitParams& InitParams)
	: ObjectReplicationBridge(InitParams.ObjectReplicationBridge)
	, ReplicationSystemInternal(InitParams.ReplicationSystemInternal)
	, LocalNetRefHandleManager(ReplicationSystemInternal->GetNetRefHandleManager())
	, NetStatsContext(nullptr)
	, ReplicatedInstances(LocalNetRefHandleManager.GetReplicatedInstances())
	, AccumulatedDirtyObjects(ReplicationSystemInternal->GetDirtyNetObjectTracker().GetAccumulatedDirtyNetObjects())
	, DirtyObjectsToQuantize(LocalNetRefHandleManager.GetDirtyObjectsToQuantize())
{
	GarbageCollectionAffectedObjects = MakeNetBitArrayView(ObjectReplicationBridge->GarbageCollectionAffectedObjects);

	// DirtyObjectsThisFrame is acquired only during polling 
}

void FObjectPoller::PreUpdatePass(const FNetBitArrayView& ObjectsConsideredForPolling)
{
	IRIS_PROFILER_SCOPE_VERBOSE(PreUpdatePass);
	NetStatsContext = ReplicationSystemInternal->GetNetTypeStats().GetNetStatsContext();

	ObjectsConsideredForPolling.ForAllSetBits([this](FInternalNetRefIndex Objectindex)
	{
		CallPreUpdate(Objectindex);
	});

	NetStatsContext = nullptr;
}

void FObjectPoller::CallPreUpdate(FInternalNetRefIndex ObjectIndex)
{
	FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (UNLIKELY(ObjectData.InstanceProtocol == nullptr))
	{
		return;
	}

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol->DebugName->Name);

	// Call per-instance PreUpdate function
	if (ObjectReplicationBridge->PreUpdateInstanceFunction && EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
	{
		UE_NET_IRIS_STATS_TIMER(Timer, NetStatsContext);
	
		ObjectReplicationBridge->PreUpdateInstanceFunction(ObjectData.RefHandle, ReplicatedInstances[ObjectIndex], ObjectReplicationBridge);
		++PollStats.PreUpdatedObjectCount;
		
		UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT(Timer, PreUpdate, ObjectIndex);
	}
}

void FObjectPoller::PollAndCopyObjects(const FNetBitArrayView& ObjectsConsideredForPolling)
{
	FDirtyObjectsAccessor DirtyObjectsAccessor(ReplicationSystemInternal->GetDirtyNetObjectTracker());
	DirtyObjectsThisFrame = DirtyObjectsAccessor.GetDirtyNetObjects();

	NetStatsContext = ReplicationSystemInternal->GetNetTypeStats().GetNetStatsContext();

	if (IsIrisPushModelEnabled())
	{
		IRIS_PROFILER_SCOPE_VERBOSE(PollAndCopyPushBased);
		ObjectsConsideredForPolling.ForAllSetBits([this](FInternalNetRefIndex Objectindex)
		{
			PushModelPollObject(Objectindex);
		});
	}
	else
	{
		IRIS_PROFILER_SCOPE_VERBOSE(ForcePollAndCopy);
		ObjectsConsideredForPolling.ForAllSetBits([this](FInternalNetRefIndex Objectindex)
		{
			ForcePollObject(Objectindex);
		});
	}

	NetStatsContext = nullptr;
}

void FObjectPoller::PollAndCopySingleObject(FNetRefHandle Handle)
{
	if (uint32 InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(Handle))
	{
		NetStatsContext = ReplicationSystemInternal->GetNetTypeStats().GetNetStatsContext();

		CallPreUpdate(InternalObjectIndex);

		FDirtyObjectsAccessor DirtyObjectsAccessor(ReplicationSystemInternal->GetDirtyNetObjectTracker());
		DirtyObjectsThisFrame = DirtyObjectsAccessor.GetDirtyNetObjects();

		ForcePollObject(InternalObjectIndex);

		// Clear ref to locked dirty bit array
		DirtyObjectsThisFrame = FNetBitArrayView();
		NetStatsContext = nullptr;
	}
}

void FObjectPoller::ForcePollObject(FInternalNetRefIndex ObjectIndex)
{
	FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (UNLIKELY(ObjectData.InstanceProtocol == nullptr))
	{
		return;
	}

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol->DebugName->Name);

	// We always poll all states here.
	ObjectData.bWantsFullPoll = 0U;

	// Poll properties if the instance protocol requires it
	if (EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll))
	{
		IRIS_PROFILER_SCOPE_VERBOSE(PollAndCopy);
		UE_NET_IRIS_STATS_TIMER(Timer, NetStatsContext);

		const bool bIsGCAffectedObject = GarbageCollectionAffectedObjects.GetBit(ObjectIndex);
		GarbageCollectionAffectedObjects.ClearBit(ObjectIndex);

		// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references
		EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
		PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

		const bool bWasAlreadyDirty = DirtyObjectsThisFrame.IsBitSet(ObjectIndex);
		const bool bPollFoundDirty = FReplicationInstanceOperations::PollAndCopyPropertyData(ObjectData.InstanceProtocol, PollOptions);
		if (bWasAlreadyDirty || bPollFoundDirty)
		{
			UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT(Timer, Poll, ObjectIndex);

			DirtyObjectsToQuantize.SetBit(ObjectIndex);
			DirtyObjectsThisFrame.SetBit(ObjectIndex);
		}
		else
		{
			UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT_AS_WASTE(Timer, Poll, ObjectIndex);
		}
		++PollStats.PolledObjectCount;
	}
	else
	{
		DirtyObjectsToQuantize.SetBit(ObjectIndex);
		DirtyObjectsThisFrame.SetBit(ObjectIndex);
	}
}

void FObjectPoller::PushModelPollObject(FInternalNetRefIndex ObjectIndex)
{
	FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (UNLIKELY(ObjectData.InstanceProtocol == nullptr))
	{
		return;
	}

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol->DebugName->Name);

	const FReplicationInstanceProtocol* InstanceProtocol = ObjectData.InstanceProtocol;

	const EReplicationInstanceProtocolTraits InstanceTraits = InstanceProtocol->InstanceTraits;
	const bool bNeedsPoll = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll);

	bool bIsDirtyObject = AccumulatedDirtyObjects.GetBit(ObjectIndex) || DirtyObjectsThisFrame.GetBit(ObjectIndex);

	if (bIsDirtyObject)
	{
		DirtyObjectsToQuantize.SetBit(ObjectIndex);
		DirtyObjectsThisFrame.SetBit(ObjectIndex);
	}

	const bool bIsGCAffectedObject = GarbageCollectionAffectedObjects.GetBit(ObjectIndex);
	GarbageCollectionAffectedObjects.ClearBit(ObjectIndex);

	// Early out if the instance does not require polling
	if (!bNeedsPoll)
	{
		return;
	}

	IRIS_PROFILER_SCOPE_VERBOSE(PollPushBased);
	UE_NET_IRIS_STATS_TIMER(Timer, NetStatsContext);

	// Does the object need to poll all states once.
	const bool bWantsFullPoll = ObjectData.bWantsFullPoll;
	ObjectData.bWantsFullPoll = 0U;
	
	// If the object is fully push model we only need to poll it if it's dirty, unless it's a new object or was garbage collected.
	bool bPollFoundDirty = false;
	if (EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
	{
		if (bIsDirtyObject || bWantsFullPoll)
		{
			// We need to do a poll if object is marked as dirty
			EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
			PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;
			bPollFoundDirty = FReplicationInstanceOperations::PollAndCopyPropertyData(InstanceProtocol, EReplicationFragmentTraits::None, PollOptions);
			++PollStats.PolledObjectCount;
		}
		else if (bIsGCAffectedObject)
		{
			// If this object might have been affected by GC, only refresh cached references
			const EReplicationFragmentTraits RequiredTraits = EReplicationFragmentTraits::HasPushBasedDirtiness;
			bPollFoundDirty = FReplicationInstanceOperations::PollAndCopyObjectReferences(InstanceProtocol, RequiredTraits);
			++PollStats.PolledReferencesObjectCount;
		}
	}
	else
	{
		// If the object has pushed based fragments, and not is marked dirty and object is affected by GC we need to make sure that we refresh cached references for all push based fragments
		const bool bIsPushBasedObject = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness);
		const bool bHasObjectReferences = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasObjectReference);
		const bool bNeedsRefreshOfCachedObjectReferences = ((!(bWantsFullPoll | bIsDirtyObject)) & bIsGCAffectedObject & bIsPushBasedObject & bHasObjectReferences);
		if (bNeedsRefreshOfCachedObjectReferences)
		{
			// Only states which has push based dirtiness need to be updated as the other states will be polled in full anyway.
			const EReplicationFragmentTraits RequiredTraits = EReplicationFragmentTraits::HasPushBasedDirtiness;
			bPollFoundDirty = FReplicationInstanceOperations::PollAndCopyObjectReferences(InstanceProtocol, RequiredTraits);
			++PollStats.PolledReferencesObjectCount;
		}

		// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references 
		EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
		PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

		// If the object is not new or dirty at this point we only need to poll non-push based fragments as we know that pushed based states have not been modified
		const EReplicationFragmentTraits ExcludeTraits = (bIsDirtyObject || bWantsFullPoll) ? EReplicationFragmentTraits::None : EReplicationFragmentTraits::HasPushBasedDirtiness;
		bPollFoundDirty |= FReplicationInstanceOperations::PollAndCopyPropertyData(InstanceProtocol, ExcludeTraits, PollOptions);
		++PollStats.PolledObjectCount;
	}

	if (bIsDirtyObject || bPollFoundDirty)
	{
		UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT(Timer, Poll, ObjectIndex);

		DirtyObjectsToQuantize.SetBit(ObjectIndex);
		DirtyObjectsThisFrame.SetBit(ObjectIndex);
	}
	else
	{
		UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT_AS_WASTE(Timer, Poll, ObjectIndex);
	}
}

} // end namespace UE::Net::Private