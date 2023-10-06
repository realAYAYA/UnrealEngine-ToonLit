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

#include "Net/Core/Trace/NetDebugName.h"

namespace UE::Net::Private
{

FObjectPoller::FObjectPoller(const FInitParams& InitParams)
	: ObjectReplicationBridge(InitParams.ObjectReplicationBridge)
	, ReplicationSystemInternal(InitParams.ReplicationSystemInternal)
	, LocalNetRefHandleManager(ReplicationSystemInternal->GetNetRefHandleManager())
	, ReplicatedInstances(LocalNetRefHandleManager.GetReplicatedInstances())
	, AccumulatedDirtyObjects(ReplicationSystemInternal->GetDirtyNetObjectTracker().GetAccumulatedDirtyNetObjects())
	, DirtyObjectsToCopy(LocalNetRefHandleManager.GetDirtyObjectsToCopy())
{
	GarbageCollectionAffectedObjects = MakeNetBitArrayView(ObjectReplicationBridge->GarbageCollectionAffectedObjects);

	// DirtyObjectsThisFrame is acquired only during polling 
}

void FObjectPoller::PollObjects(const FNetBitArrayView& ObjectsConsideredForPolling)
{
	FDirtyObjectsAccessor DirtyObjectsAccessor(ReplicationSystemInternal->GetDirtyNetObjectTracker());
	DirtyObjectsThisFrame = DirtyObjectsAccessor.GetDirtyNetObjects();

	// From here we call into user code via PreUpdateInstanceFunction, so allow external code to set dirty flags since DirtyObjects is not read anymore.
	ReplicationSystemInternal->GetDirtyNetObjectTracker().AllowExternalAccess();

	if (IsIrisPushModelEnabled())
	{
		ObjectsConsideredForPolling.ForAllSetBits([this](FInternalNetRefIndex Objectindex)
		{
			PushModelPollObject(Objectindex);
		});
	}
	else
	{
		ObjectsConsideredForPolling.ForAllSetBits([this](FInternalNetRefIndex Objectindex)
		{
			ForcePollObject(Objectindex);
		});
	}

	// Clear ref to locked dirty bit array
	DirtyObjectsThisFrame = FNetBitArrayView();

	ReplicationSystemInternal->GetDirtyNetObjectTracker().SetCurrentPolledObject(FNetRefHandleManager::InvalidInternalIndex);
}

void FObjectPoller::PollSingleObject(FNetRefHandle Handle)
{
	if (uint32 InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(Handle))
	{
		FDirtyObjectsAccessor DirtyObjectsAccessor(ReplicationSystemInternal->GetDirtyNetObjectTracker());
		DirtyObjectsThisFrame = DirtyObjectsAccessor.GetDirtyNetObjects();

		// From here we call into user code via PreUpdateInstanceFunction, so allow external code to set dirty flags since DirtyObjects is not read anymore.
		ReplicationSystemInternal->GetDirtyNetObjectTracker().AllowExternalAccess();

		ForcePollObject(InternalObjectIndex);

		// Clear ref to locked dirty bit array
		DirtyObjectsThisFrame = FNetBitArrayView();
	}
}

void FObjectPoller::ForcePollObject(FInternalNetRefIndex ObjectIndex)
{
	FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (ObjectData.InstanceProtocol == nullptr)
	{
		return;
	}

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol->DebugName->Name);

	// We always poll all states here.
	ObjectData.bWantsFullPoll = 0U;

	// Call per-instance PreUpdate function
	if (ObjectReplicationBridge->PreUpdateInstanceFunction && EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
	{
		IRIS_PROFILER_SCOPE_VERBOSE(PreReplicationUpdate);
		ObjectReplicationBridge->PreUpdateInstanceFunction(ObjectData.RefHandle, ReplicatedInstances[ObjectIndex], ObjectReplicationBridge);
		++PollStats.PreUpdatedObjectCount;
	}

	// Poll properties if the instance protocol requires it
	if (EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll))
	{
		IRIS_PROFILER_SCOPE_VERBOSE(Poll);

		const bool bIsGCAffectedObject = GarbageCollectionAffectedObjects.GetBit(ObjectIndex);
		GarbageCollectionAffectedObjects.ClearBit(ObjectIndex);

		// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references
		EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
		PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

		const bool bWasAlreadyDirty = DirtyObjectsThisFrame.IsBitSet(ObjectIndex);
		const bool bPollFoundDirty = FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(ObjectData.InstanceProtocol, PollOptions);
		if (bWasAlreadyDirty || bPollFoundDirty)
		{
			DirtyObjectsToCopy.SetBit(ObjectIndex);
			DirtyObjectsThisFrame.SetBit(ObjectIndex);
		}
		++PollStats.PolledObjectCount;
	}
	else
	{
		DirtyObjectsToCopy.SetBit(ObjectIndex);
		DirtyObjectsThisFrame.SetBit(ObjectIndex);
	}
}

void FObjectPoller::PushModelPollObject(FInternalNetRefIndex ObjectIndex)
{
	FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (ObjectData.InstanceProtocol == nullptr)
	{
		return;
	}

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol->DebugName->Name);

	ReplicationSystemInternal->GetDirtyNetObjectTracker().SetCurrentPolledObject(ObjectIndex);

	const FReplicationInstanceProtocol* InstanceProtocol = ObjectData.InstanceProtocol;

	const EReplicationInstanceProtocolTraits InstanceTraits = InstanceProtocol->InstanceTraits;
	const bool bNeedsPoll = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll);

	bool bIsDirtyObject = AccumulatedDirtyObjects.GetBit(ObjectIndex);

	// Call per-instance PreUpdate function
	if (ObjectReplicationBridge->PreUpdateInstanceFunction && EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
	{
		IRIS_PROFILER_SCOPE_VERBOSE(PreReplicationUpdate);

		ObjectReplicationBridge->PreUpdateInstanceFunction(ObjectData.RefHandle, ReplicatedInstances[ObjectIndex], ObjectReplicationBridge);
		++PollStats.PreUpdatedObjectCount;

		// Pre update may have called MarkDirty. Detect it.
		bIsDirtyObject = bIsDirtyObject || DirtyObjectsThisFrame.GetBit(ObjectIndex);
		if (bNeedsPoll && !bIsDirtyObject)
		{
			bIsDirtyObject = FGlobalDirtyNetObjectTracker::IsNetObjectStateDirty(ObjectData.NetHandle);
		}

		if (bIsDirtyObject)
		{
			DirtyObjectsToCopy.SetBit(ObjectIndex);
			DirtyObjectsThisFrame.SetBit(ObjectIndex);
		}
	}
	else if (bIsDirtyObject)
	{
		DirtyObjectsToCopy.SetBit(ObjectIndex);
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
			bPollFoundDirty = FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(InstanceProtocol, EReplicationFragmentTraits::None, PollOptions);
			++PollStats.PolledObjectCount;
		}
		else if (bIsGCAffectedObject)
		{
			// If this object might have been affected by GC, only refresh cached references
			const EReplicationFragmentTraits RequiredTraits = EReplicationFragmentTraits::HasPushBasedDirtiness;
			bPollFoundDirty = FReplicationInstanceOperations::PollAndRefreshCachedObjectReferences(InstanceProtocol, RequiredTraits);
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
			bPollFoundDirty = FReplicationInstanceOperations::PollAndRefreshCachedObjectReferences(InstanceProtocol, RequiredTraits);
			++PollStats.PolledReferencesObjectCount;
		}

		// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references 
		EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
		PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

		// If the object is not new or dirty at this point we only need to poll non-push based fragments as we know that pushed based states have not been modified
		const EReplicationFragmentTraits ExcludeTraits = (bIsDirtyObject || bWantsFullPoll) ? EReplicationFragmentTraits::None : EReplicationFragmentTraits::HasPushBasedDirtiness;
		bPollFoundDirty |= FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(InstanceProtocol, ExcludeTraits, PollOptions);
		++PollStats.PolledObjectCount;
	}

	if (bIsDirtyObject || bPollFoundDirty)
	{
		DirtyObjectsToCopy.SetBit(ObjectIndex);
		DirtyObjectsThisFrame.SetBit(ObjectIndex);
	}
}

} // end namespace UE::Net::Private