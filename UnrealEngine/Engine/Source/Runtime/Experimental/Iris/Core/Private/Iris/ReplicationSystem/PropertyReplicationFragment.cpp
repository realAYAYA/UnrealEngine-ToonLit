// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/PropertyReplicationFragment.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/IrisLog.h"

namespace UE::Net
{

FPropertyReplicationFragment::FPropertyReplicationFragment(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor)
: FReplicationFragment(InTraits)
, ReplicationStateDescriptor(InDescriptor)
, Owner(InOwner)
{
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReplicate))
	{
		SrcReplicationState = MakeUnique<FPropertyReplicationState>(InDescriptor);
	}
	
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReceive))
	{
		if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasRepNotifies))
		{
			if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::KeepPreviousState))
			{
				PrevReplicationState = MakeUnique<FPropertyReplicationState>(InDescriptor);

				// Poll to get instance default for our previous state
				PrevReplicationState->PollPropertyReplicationState(InOwner);

				Traits |= EReplicationFragmentTraits::KeepPreviousState;
			}

			Traits |= EReplicationFragmentTraits::HasRepNotifies;
		}

		// For now we always expect pre/post operations for legacy states, we might make this
		Traits |= EReplicationFragmentTraits::NeedsLegacyCallbacks;
	}
	
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReplicate))
	{
		// For PropertyReplicationStates we need to poll properties from our owner in order to detect state changes.
		Traits |= EReplicationFragmentTraits::NeedsPoll;
	}

	// Propagate push based dirtiness.
	if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasPushBasedDirtiness))
	{
		Traits |= EReplicationFragmentTraits::HasPushBasedDirtiness;
	}

	// Propagate object reference.
	if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasObjectReference))
	{
		Traits |= EReplicationFragmentTraits::HasObjectReference;
	}

	Traits |= EReplicationFragmentTraits::HasPropertyReplicationState;
}

FPropertyReplicationFragment::~FPropertyReplicationFragment() = default;

void FPropertyReplicationFragment::Register(FFragmentRegistrationContext& Context)
{
	Context.RegisterReplicationFragment(this, ReplicationStateDescriptor.GetReference(), SrcReplicationState ? SrcReplicationState->GetStateBuffer() : nullptr);
}

void FPropertyReplicationFragment::CollectOwner(FReplicationStateOwnerCollector* Owners) const
{
	Owners->AddOwner(Owner);
}

void FPropertyReplicationFragment::CallRepNotifies(FReplicationStateApplyContext& Context)
{
	IRIS_PROFILER_SCOPE(PropertyReplicationFragment_InvokeRepNotifies);

	const FPropertyReplicationState ReceivedState(Context.Descriptor, Context.StateBufferData.ExternalStateBuffer);

	if (PrevReplicationState || !Context.bIsInit)
	{
		ReceivedState.CallRepNotifies(Owner, PrevReplicationState.Get(), Context.bIsInit);

		// We keep a copy of the previous state for RepNotifies that need the value
		if (PrevReplicationState)
		{
			*PrevReplicationState = ReceivedState;
		}
	}
	else
	{
		// As our default initial states is always treated as all dirty we need to compare against the default before applying initial repnotifies
		const FPropertyReplicationState DefaultState(Context.Descriptor);

		ReceivedState.CallRepNotifies(Owner, &DefaultState, Context.bIsInit);
	}
}

void FPropertyReplicationFragment::ApplyReplicatedState(FReplicationStateApplyContext& Context) const
{
	IRIS_PROFILER_SCOPE(PropertyReplicationFragment_ApplyReplicatedState);

	// Create a wrapping property replication state, cheap as we are simply injecting the already constructed state
	const FPropertyReplicationState ReceivedState(Context.Descriptor, Context.StateBufferData.ExternalStateBuffer);

	// Just push the state data to owner
	ReceivedState.PushPropertyReplicationState(Owner);
}

void FPropertyReplicationFragment::PollReplicatedState(EReplicationFragmentPollFlags PollOption)
{
	// Since PollPropertyReplicationState always copies the new value we do not need to do anything special
	// with object references if the EReplicationFragmentPollOption::All flag is set
	if (EnumHasAnyFlags(PollOption, EReplicationFragmentPollFlags::PollAllState))
	{
		const void* ExternalSourceState = static_cast<void*>(Owner);
		SrcReplicationState->PollPropertyReplicationState(ExternalSourceState);
	}
	else if (EnumHasAnyFlags(PollOption, EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC))
	{
		const void* ExternalSourceState = static_cast<void*>(Owner);
		SrcReplicationState->PollObjectReferences(ExternalSourceState);
	}
}

void FPropertyReplicationFragment::ReplicatedStateToString(FStringBuilderBase& StringBuilder, FReplicationStateApplyContext& Context, EReplicationStateToStringFlags Flags) const
{
	const FPropertyReplicationState ReceivedState(Context.Descriptor, Context.StateBufferData.ExternalStateBuffer);

	const bool bIncludeAll = EnumHasAnyFlags(Flags, EReplicationStateToStringFlags::OnlyIncludeDirtyMembers) == false;
	ReceivedState.ToString(StringBuilder, bIncludeAll);
};

FPropertyReplicationFragment* FPropertyReplicationFragment::CreateAndRegisterFragment(UObject* InOwner, const FReplicationStateDescriptor* InDescriptor, FFragmentRegistrationContext& Context)
{
	// Fast arrays needs to be bound explicitly
	if (InDescriptor == nullptr || EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::IsFastArrayReplicationState))
	{
		return nullptr;
	}
	
	FPropertyReplicationFragment* Fragment = new FPropertyReplicationFragment(Context.GetFragmentTraits(), InOwner, InDescriptor);

	Fragment->Traits |= EReplicationFragmentTraits::DeleteWithInstanceProtocol;

	Fragment->Register(Context);

	return Fragment;
}

}
