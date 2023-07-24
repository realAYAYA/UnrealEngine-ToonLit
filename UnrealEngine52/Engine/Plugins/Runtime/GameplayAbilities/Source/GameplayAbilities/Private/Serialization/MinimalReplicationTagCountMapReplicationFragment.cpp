// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/MinimalReplicationTagCountMapReplicationFragment.h"

#if UE_WITH_IRIS

#include "Engine/NetConnection.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/NetSerializer.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"

namespace UE::Net
{

FReplicationFragment* CreateAndRegisterMinimalReplicationTagCountMapReplicationFragment(UObject* Owner, const FReplicationStateDescriptor* Descriptor, FFragmentRegistrationContext& Context)
{
	if (FMinimalReplicationTagCountMapReplicationFragment* Fragment = new FMinimalReplicationTagCountMapReplicationFragment(Context.GetFragmentTraits() | EReplicationFragmentTraits::DeleteWithInstanceProtocol, Owner, Descriptor))
	{
		Fragment->Register(Context);
		return Fragment;
	}

	return nullptr;
}

FMinimalReplicationTagCountMapReplicationFragment::FMinimalReplicationTagCountMapReplicationFragment(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor)
: FReplicationFragment(InTraits)
, ReplicationStateDescriptor(InDescriptor)
, Owner(InOwner)
{
	// We don't want to create temporary states when applying replicated data as we only want to update the replicated fields.
	Traits |= EReplicationFragmentTraits::HasPersistentTargetStateBuffer;

	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReplicate))
	{
		Traits |= EReplicationFragmentTraits::NeedsPoll;

		// We assume the struct only replicates a few members and the serializer knows which ones. We don't want to mark the struct as dirty if it isn't.
		check(EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::UseSerializerIsEqual));

		SrcReplicationState = MakeUnique<FPropertyReplicationState>(InDescriptor);
	}
	
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReceive))
	{
		if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasRepNotifies))
		{
			ensureMsgf(!EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::KeepPreviousState), TEXT("FMinimalReplicationTagCountMapReplicationFragment doesn't support OnRep calls with previous value."));
		}

		// We do custom callbacks in CallRepNotifies rather than in ApplyReplicatedState.
		Traits |= EReplicationFragmentTraits::HasRepNotifies | EReplicationFragmentTraits::NeedsLegacyCallbacks;
	}
}

void FMinimalReplicationTagCountMapReplicationFragment::Register(FFragmentRegistrationContext& Context)
{
	FPropertyReplicationState* ReplicationState = SrcReplicationState.Get();
	Context.RegisterReplicationFragment(this, ReplicationStateDescriptor.GetReference(), (ReplicationState ? ReplicationState->GetStateBuffer() : nullptr));
}

void FMinimalReplicationTagCountMapReplicationFragment::ApplyReplicatedState(FReplicationStateApplyContext& ApplyContext) const
{
	uint8* ExternalStatePointer = reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC();

	// Clear tag counts in the map. This information is used in MimicMinimalReplicationTagCountMapReceiveLogic.
	FMinimalReplicationTagCountMap* ExternalSourceState = reinterpret_cast<FMinimalReplicationTagCountMap*>(ExternalStatePointer);
	for (auto& Pair : ExternalSourceState->TagMap)
	{
		Pair.Value = 0;
	}

	// Dequantize replicated members to external source state. Need to call the serializer directly with the appropriate arguments as the external state doesn't have a ReplicationStateHeader in front of it.
	FNetDequantizeArgs DequantizeArgs = {};
	DequantizeArgs.Source = NetSerializerValuePointer(ApplyContext.StateBufferData.RawStateBuffer);
	DequantizeArgs.Target = NetSerializerValuePointer(ExternalSourceState);
	DequantizeArgs.NetSerializerConfig = ReplicationStateDescriptor->MemberSerializerDescriptors[0].SerializerConfig;
	const FNetSerializer* Serializer = ReplicationStateDescriptor->MemberSerializerDescriptors[0].Serializer;
	Serializer->Dequantize(*ApplyContext.NetSerializationContext, DequantizeArgs);
}

bool FMinimalReplicationTagCountMapReplicationFragment::PollReplicatedState(EReplicationFragmentPollFlags PollOption)
{
	if (EnumHasAnyFlags(PollOption, EReplicationFragmentPollFlags::PollAllState))
	{
		const uint8* ExternalStateBuffer = reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC();
		const FMinimalReplicationTagCountMap* ExternalSourceState = reinterpret_cast<const FMinimalReplicationTagCountMap*>(ExternalStateBuffer);

		const void* CachedStateBuffer = SrcReplicationState->GetStateBuffer();
		const FMinimalReplicationTagCountMap* CachedState = reinterpret_cast<const FMinimalReplicationTagCountMap*>(static_cast<const uint8*>(CachedStateBuffer) + ReplicationStateDescriptor->MemberDescriptors[0].ExternalMemberOffset);

		return SrcReplicationState->PollPropertyReplicationState(Owner);
	}

	return SrcReplicationState->IsDirty(0);
}

void FMinimalReplicationTagCountMapReplicationFragment::CallRepNotifies(FReplicationStateApplyContext& Context)
{
	MimicMinimalReplicationTagCountMapReceiveLogic(Context);
	CallRepNotify(Context);
}

void FMinimalReplicationTagCountMapReplicationFragment::MimicMinimalReplicationTagCountMapReceiveLogic(FReplicationStateApplyContext& Context)
{
	uint8* ExternalStatePointer = reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC();
	FMinimalReplicationTagCountMap* ExternalSourceState = reinterpret_cast<FMinimalReplicationTagCountMap*>(ExternalStatePointer);

	// Check whether we need to perform the complex logic or not.
	UAbilitySystemComponent* StateOwner = ExternalSourceState->Owner;
	bool UpdateOwnerTagMap = StateOwner != nullptr;
	if (ExternalSourceState->bRequireNonOwningNetConnection && StateOwner)
	{
		if (AActor* OwningActor = StateOwner->GetOwner())
		{			
			if (const UNetConnection* OwnerNetConnection = OwningActor->GetNetConnection())
			{
				if (OwnerNetConnection->GetConnectionId() == Context.NetSerializationContext->GetLocalConnectionId())
				{
					UpdateOwnerTagMap = false;
				}
			}
		}
	}

	if (!UpdateOwnerTagMap)
	{
		return;
	}

	ExternalSourceState->UpdateOwnerTagMap();
}

void FMinimalReplicationTagCountMapReplicationFragment::CallRepNotify(FReplicationStateApplyContext& ApplyContext)
{
	if (const UFunction* RepNotifyFunction = ReplicationStateDescriptor->MemberPropertyDescriptors[0].RepNotifyFunction)
	{
		if (ApplyContext.bIsInit)
		{
			const uint8* ReceivedState = ApplyContext.StateBufferData.RawStateBuffer;
			const uint8* DefaultState = ReplicationStateDescriptor->DefaultStateBuffer;

			if (FReplicationStateOperations::IsEqualQuantizedState(*ApplyContext.NetSerializationContext, ReceivedState, DefaultState, ReplicationStateDescriptor))
			{
				return;
			}
		}

		Owner->ProcessEvent(const_cast<UFunction*>(RepNotifyFunction), nullptr);
	}
}

}

#endif
