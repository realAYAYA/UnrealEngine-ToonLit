// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/Private/FastArrayReplicationFragmentInternal.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"

namespace UE::Net
{

/**
 * TFastArrayReplicationFragment - Binds a typed FastArray to a FReplicationfragments
 * Used to support FFastArray-based serialization with no required code modifications
 * Backed by a PropertyReplicationState which means that we will have to poll source data for dirtiness,
 * in the case of FastArrays this involves comparing the replication key of the array and its items.
 */
template <typename FastArrayItemType, typename FastArrayType>
class TFastArrayReplicationFragment final : public Private::FFastArrayReplicationFragmentBase
{
public:
	typedef TArray<FastArrayItemType> ItemArrayType;
	TFastArrayReplicationFragment(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor) : FFastArrayReplicationFragmentBase(InTraits, InOwner, InDescriptor) {}

protected:
	virtual void ApplyReplicatedState(FReplicationStateApplyContext& Context) const override;
	virtual void PollReplicatedState(EReplicationFragmentPollFlags PollOption) override;

	void PollAllState(bool bForceFullCompare = false);

private:
	// Get the wrapped FastArraySerializerProperty
	inline FastArrayType* GetWrappedFastArraySerializer() const;
};

/**
 * TNativeFastArrayReplicationFragment - Binds a typed FastArray to a FReplicationfragments
 * Used to support FFastArray-based serialization with some minor code modifications
 * The FastArray must be changed to inherit from IrisFastArraySerializer instead of FFastArraySerializer which will inject a
 * ReplicationStateHeader and a fixed size changemask, in it most basic form this allows us to not keep a full copy the fast array to detect dirtiness 
 * but instead only store the ReplicationID and ReplicationKeys. We can also provide an alternative interface for editing the FastArrays which allows us 
 * update dirtiness directly and skip the poll step completely.
 */
template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType = FastArrayPollingPolicies::FNoPollingPolicy>
class TNativeFastArrayReplicationFragment final : public Private::FNativeFastArrayReplicationFragmentBase
{
public:
	typedef TArray<FastArrayItemType> ItemArrayType;

	TNativeFastArrayReplicationFragment(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor);
	void Register(FFragmentRegistrationContext& Fragments, EReplicationFragmentTraits Traits = EReplicationFragmentTraits::None);

protected:
	// FReplicationFragment implementation
	virtual void ApplyReplicatedState(FReplicationStateApplyContext& Context) const override;
	virtual void CallRepNotifies(FReplicationStateApplyContext& Context) override;
	virtual void ReplicatedStateToString(FStringBuilderBase& StringBuilder, FReplicationStateApplyContext& Context, EReplicationStateToStringFlags Flags) const override;
	virtual void PollReplicatedState(EReplicationFragmentPollFlags PollOption) override;

	void PollAllState();

private:
	// Get the wrapped FastArraySerializerProperty
	inline FastArrayType* GetWrappedFastArraySerializer() const;
	PollingPolicyType PollingPolicy;
};

/**
 * TFastArrayReplicationFragment implementation
 */
template <typename FastArrayItemType, typename FastArrayType>
void TFastArrayReplicationFragment<FastArrayItemType, FastArrayType>::ApplyReplicatedState(FReplicationStateApplyContext& Context) const
{
	// Get the wrapped FastArraySerializer and array
	FastArrayType* DstArraySerializer = GetWrappedFastArraySerializer();
	ItemArrayType* DstWrappedArray = reinterpret_cast<ItemArrayType*>(reinterpret_cast<uint8*>(DstArraySerializer) + WrappedArrayOffsetRelativeFastArraySerializerProperty);

	// Intentionally not const as we allow the src state to be modified
	FastArrayType* SrcArraySerializer;
	const ItemArrayType* SrcWrappedArray;

	// Get received array data
	{
		uint8* ReceivedStateBuffer = Context.StateBufferData.ExternalStateBuffer;
		const SIZE_T FastArraySerializerMemberOffset = ReplicationStateDescriptor->MemberDescriptors[0].ExternalMemberOffset;
		SrcArraySerializer = reinterpret_cast<FastArrayType*>(ReceivedStateBuffer + FastArraySerializerMemberOffset);
		SrcWrappedArray = reinterpret_cast<const ItemArrayType*>(ReceivedStateBuffer + FastArraySerializerMemberOffset + WrappedArrayOffsetRelativeFastArraySerializerProperty);
	}

	// Apply state and issue callbacks etc
	Private::FFastArrayReplicationFragmentHelper::ApplyReplicatedState(DstArraySerializer, DstWrappedArray, SrcArraySerializer, SrcWrappedArray, GetArrayElementDescriptor(), Context);
}

template <typename FastArrayItemType, typename FastArrayType>
FastArrayType* TFastArrayReplicationFragment<FastArrayItemType, FastArrayType>::GetWrappedFastArraySerializer() const
{
	return reinterpret_cast<FastArrayType*>(reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC());
}

template <typename FastArrayItemType, typename FastArrayType>
void TFastArrayReplicationFragment<FastArrayItemType, FastArrayType>::PollReplicatedState(EReplicationFragmentPollFlags PollOption)
{
	// If the ForceObjectReferences flag is set we cannot early out and must always refresh cached data
	if (EnumHasAnyFlags(PollOption, EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC))
	{
		PollAllState(true);
	}
	else
	{
		PollAllState(false);
	}
}

template <typename FastArrayItemType, typename FastArrayType>
void TFastArrayReplicationFragment<FastArrayItemType, FastArrayType>::PollAllState(bool bForceFullCompare)
{
	// Lookup source data, we need the actual FastArraySerializer and the Array it is wrapping
	FastArrayType* SrcArraySerializer = GetWrappedFastArraySerializer();
	ItemArrayType* SrcWrappedArray = reinterpret_cast<ItemArrayType*>(reinterpret_cast<uint8*>(SrcArraySerializer) + WrappedArrayOffsetRelativeFastArraySerializerProperty);
	
	// Lookup destination data
	FastArrayType* DstArraySerializer = reinterpret_cast<FastArrayType*>(SrcReplicationState->GetStateBuffer() + ReplicationStateDescriptor->MemberDescriptors[0].ExternalMemberOffset);
	ItemArrayType* DstWrappedArray = reinterpret_cast<ItemArrayType*>(reinterpret_cast<uint8*>(DstArraySerializer) + WrappedArrayOffsetRelativeFastArraySerializerProperty);

	// Check if we can early out
	if (!bForceFullCompare && SrcArraySerializer->ArrayReplicationKey == DstArraySerializer->ArrayReplicationKey)
	{
		return;
	}

	// First we must resize target to match size of source data
	bool bMarkArrayDirty = false;
	const uint32 ElementCount = SrcWrappedArray->Num();
	if (DstWrappedArray->Num() != ElementCount)
	{
		DstWrappedArray->SetNum(ElementCount);
		bMarkArrayDirty = true;
	}

	const FReplicationStateDescriptor* ArrayElementDescriptor = GetArrayElementDescriptor();
	const FReplicationStateMemberDescriptor* MemberDescriptors = ArrayElementDescriptor->MemberDescriptors;

	// We currently use a simple modulo scheme for bits in the changemask
	// A single bit might represent several entries in the array which all will be considered dirty, it is up to the serializer to handle this
	// The first bit is used by the owning property we need to offset by one and deduct one from the usable bits
	FNetBitArrayView MemberChangeMask = UE::Net::Private::GetMemberChangeMask(SrcReplicationState->GetStateBuffer(), ReplicationStateDescriptor);
	
	const FReplicationStateMemberChangeMaskDescriptor& MemberChangeMaskDescriptor = ReplicationStateDescriptor->MemberChangeMaskDescriptors[0];
	const uint32 ChangeMaskBitOffset = MemberChangeMaskDescriptor.BitOffset + FIrisFastArraySerializer::IrisFastArrayChangeMaskBitOffset;
	const uint32 ChangeMaskBitCount = MemberChangeMaskDescriptor.BitCount - FIrisFastArraySerializer::IrisFastArrayChangeMaskBitOffset;

	FastArrayItemType* DstItems = DstWrappedArray->GetData();
	FastArrayItemType* SrcItems = SrcWrappedArray->GetData();

	// Iterate over array entries and copy the statedata using internal data if it has changed
	for (int32 ElementIt = 0, ElementEndIt = ElementCount; ElementIt < ElementEndIt; ++ElementIt)
	{
		FastArrayItemType& SrcItem = SrcItems[ElementIt];
		FastArrayItemType& DstItem = DstItems[ElementIt];

		const bool bIsWritingOnClient = false;
		if (SrcArraySerializer->template ShouldWriteFastArrayItem<FastArrayItemType, FastArrayType>(SrcItem, bIsWritingOnClient))
		{
			if (SrcItem.ReplicationID == INDEX_NONE)
			{
				SrcArraySerializer->MarkItemDirty(SrcItem);
			}

			const bool bReplicationKeyChanged = SrcItem.ReplicationKey != DstItem.ReplicationKey || SrcItem.ReplicationID != DstItem.ReplicationID;
			if (bReplicationKeyChanged || (bForceFullCompare && !InternalCompareArrayElement(ArrayElementDescriptor, &DstItem, &SrcItem)))
			{
				InternalCopyArrayElement(ArrayElementDescriptor, &DstItem, &SrcItem);
				DstItem.ReplicationKey = SrcItem.ReplicationKey;

				// Mark element as dirty and mark array as dirty as well.
				if (ChangeMaskBitCount)
				{
					MemberChangeMask.SetBit((ElementIt % ChangeMaskBitCount) + ChangeMaskBitOffset);
				}
				bMarkArrayDirty = true;
			}
		}
	}

	// We update this after the poll since every call to MarkItem() dirty will Increase the ArrayReplicationKey
	DstArraySerializer->ArrayReplicationKey = SrcArraySerializer->ArrayReplicationKey;

	// Mark the NetObject as dirty
	if (bMarkArrayDirty && SrcReplicationState->IsCustomConditionEnabled(FIrisFastArraySerializer::IrisFastArrayPropertyBitIndex))
	{
		MemberChangeMask.SetBit(FIrisFastArraySerializer::IrisFastArrayPropertyBitIndex);
		MarkNetObjectStateDirty(UE::Net::Private::GetReplicationStateHeader(SrcReplicationState->GetStateBuffer(), ReplicationStateDescriptor));
	}
}

/*
 * TNativeFastArrayReplicationFragment implementation
 */
template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::TNativeFastArrayReplicationFragment(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor)
: FNativeFastArrayReplicationFragmentBase(InTraits, InOwner, InDescriptor)
{
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReplicate) && (PollingPolicy.GetPollingState() != nullptr))
	{
		// For PropertyReplicationStates we need to poll properties from our owner in order to detect state changes.
		Traits |= EReplicationFragmentTraits::NeedsPoll;
	}
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::Register(FFragmentRegistrationContext& Context, EReplicationFragmentTraits InTraits)
{
	Traits |= InTraits;
	Context.RegisterReplicationFragment(this, ReplicationStateDescriptor.GetReference(), reinterpret_cast<uint8*>(GetWrappedFastArraySerializer()));
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::CallRepNotifies(FReplicationStateApplyContext& Context)
{
	const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;

	// if we get here we are either the init state or dirty
	if (const UFunction* RepNotifyFunction = Descriptor->MemberPropertyDescriptors[0].RepNotifyFunction)
	{
		// if this is the init state, we compare against default and early out if initial state does not differ from default (empty)
		if (Context.bIsInit)
		{
			FastArrayType ReceivedState;
			FastArrayType DefaultState;
			InternalDequantizeFastArray(*Context.NetSerializationContext, reinterpret_cast<uint8*>(&ReceivedState), Context.StateBufferData.RawStateBuffer, GetFastArraySerializerPropertyDescriptor());
			if (Descriptor->MemberProperties[0]->Identical(&ReceivedState, &DefaultState))
			{
				return;
			}
		}

		Owner->ProcessEvent(const_cast<UFunction*>(RepNotifyFunction), nullptr);
	}
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::PollReplicatedState(EReplicationFragmentPollFlags PollOption)
{
	// We ignore object references polling. Since the source state will have references cleaned up they will be valid once any affected item is dirtied.
	if (PollOption == EReplicationFragmentPollFlags::PollAllState)
	{
		PollAllState();
	}
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::PollAllState()
{
	using FPollingState = FastArrayPollingPolicies::FPollingState;
	if (FPollingState* PollingState = PollingPolicy.GetPollingState())
	{
		// Get the source FastArraySerializer and array
		FastArrayType* SrcArraySerializer = GetWrappedFastArraySerializer();
		ItemArrayType* SrcWrappedArray = reinterpret_cast<ItemArrayType*>(reinterpret_cast<uint8*>(SrcArraySerializer) + WrappedArrayOffsetRelativeFastArraySerializerProperty);

		// Check if we can early out
		if (SrcArraySerializer->ArrayReplicationKey == PollingState->ArrayReplicationKey)
		{
			return;
		}

		// First we must resize target to match size of source data
		bool bMarkArrayDirty = false;
		const uint32 ElementCount = SrcWrappedArray->Num();
		if (PollingState->ItemPollData.Num() != ElementCount)
		{
			PollingState->ItemPollData.SetNum(ElementCount);
			bMarkArrayDirty = true;
		}

		FNetBitArrayView MemberChangeMask = UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::GetChangeMask(*SrcArraySerializer);

		// We currently use a simple modulo scheme for bits in the changemask
		// A single bit might represent several entries in the array which all will be considered dirty, it is up to the serializer to handle this
		// The first bit is used by the owning property we need to offset by one and deduct one from the usable bits
		const FReplicationStateMemberChangeMaskDescriptor& MemberChangeMaskDescriptor = ReplicationStateDescriptor->MemberChangeMaskDescriptors[0];
		const uint32 ChangeMaskBitOffset = MemberChangeMaskDescriptor.BitOffset + FIrisFastArraySerializer::IrisFastArrayChangeMaskBitOffset;
		const uint32 ChangeMaskBitCount = MemberChangeMaskDescriptor.BitCount - FIrisFastArraySerializer::IrisFastArrayChangeMaskBitOffset;

		FPollingState::FEntry* DstItems = PollingState->ItemPollData.GetData();
		FastArrayItemType* SrcItems = SrcWrappedArray->GetData();

		// Iterate over array entries and copy the statedata using internal data if it has changed
		for (uint32 ElementIt = 0, ElementEndIt = ElementCount; ElementIt < ElementEndIt; ++ElementIt)
		{
			FastArrayItemType& SrcItem = SrcItems[ElementIt];
			FPollingState::FEntry& DstItem = DstItems[ElementIt];

			const bool bIsWritingOnClient = false;
			if (SrcArraySerializer->template ShouldWriteFastArrayItem<FastArrayItemType, FastArrayType>(SrcItem, bIsWritingOnClient))
			{
				if (SrcItem.ReplicationID == INDEX_NONE)
				{
					SrcArraySerializer->MarkItemDirty(SrcItem);
				}

				if (SrcItem.ReplicationKey != DstItem.ReplicationKey || SrcItem.ReplicationID != DstItem.ReplicationID)
				{
					DstItem.ReplicationKey = SrcItem.ReplicationKey;
					DstItem.ReplicationID = SrcItem.ReplicationID;

					// Mark element as dirty and mark array as dirty as well.
					if (ChangeMaskBitCount)
					{
						MemberChangeMask.SetBit((ElementIt % ChangeMaskBitCount) + ChangeMaskBitOffset);
					}
					bMarkArrayDirty = true;
				}
			}
		}

		// We update this after the poll since every call to MarkItem() dirty will Increase the ArrayReplicationKey
		PollingState->ArrayReplicationKey = SrcArraySerializer->ArrayReplicationKey;

		if (bMarkArrayDirty)
		{
			const bool bHasCustomConditionals = EnumHasAnyFlags(ReplicationStateDescriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals);
			if (!bHasCustomConditionals || UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::GetConditionalChangeMask(*SrcArraySerializer).GetBit(FIrisFastArraySerializer::IrisFastArrayPropertyBitIndex))
			{
				UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::MarkArrayDirty(*SrcArraySerializer);
			}
		}
	}
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::ApplyReplicatedState(FReplicationStateApplyContext& Context) const
{
	// As we must preserve local data and generate proper callbacks we must dequantize into a temporary state
	// We could do a selective operation and keep an targetstate around and only do incremental updates of this state
	FastArrayType ReceivedState;

	// Dequantize into temporary array
	InternalDequantizeFastArray(*Context.NetSerializationContext, reinterpret_cast<uint8*>(&ReceivedState), Context.StateBufferData.RawStateBuffer, GetFastArraySerializerPropertyDescriptor());

	// Get the wrapped FastArraySerializer and array
	FastArrayType* DstArraySerializer = reinterpret_cast<FastArrayType*>(reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC());
	ItemArrayType* DstWrappedArray = reinterpret_cast<ItemArrayType*>((uint8*)(DstArraySerializer) + WrappedArrayOffsetRelativeFastArraySerializerProperty);

	// Intentionally not const as we allow the src state to be modified
	FastArrayType* SrcArraySerializer = &ReceivedState;
	const ItemArrayType* SrcWrappedArray = reinterpret_cast<const ItemArrayType*>(reinterpret_cast<uint8*>(&ReceivedState) + WrappedArrayOffsetRelativeFastArraySerializerProperty);	

	// Apply state and issue callbacks etc
	Private::FFastArrayReplicationFragmentHelper::ApplyReplicatedState(DstArraySerializer, DstWrappedArray, SrcArraySerializer, SrcWrappedArray, GetArrayElementDescriptor(), Context);
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
void TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::ReplicatedStateToString(FStringBuilderBase& StringBuilder, FReplicationStateApplyContext& Context, EReplicationStateToStringFlags Flags) const
{
	// Create temporary
	FastArrayType ReceivedState;

	// Dequantize into temporary array
	InternalDequantizeFastArray(*Context.NetSerializationContext, reinterpret_cast<uint8*>(&ReceivedState), Context.StateBufferData.RawStateBuffer, GetFastArraySerializerPropertyDescriptor());

	// Output state to string
	ToString(StringBuilder, reinterpret_cast<uint8*>(&ReceivedState), GetFastArraySerializerPropertyDescriptor());
}

template <typename FastArrayItemType, typename FastArrayType, typename PollingPolicyType>
FastArrayType* TNativeFastArrayReplicationFragment<FastArrayItemType, FastArrayType, PollingPolicyType>::GetWrappedFastArraySerializer() const
{
	return reinterpret_cast<FastArrayType*>(reinterpret_cast<uint8*>(Owner) + ReplicationStateDescriptor->MemberProperties[0]->GetOffset_ForGC());
}

namespace Private {

template <typename FastArrayType>
FReplicationFragment* CreateAndRegisterFragment(UObject* Owner, const FReplicationStateDescriptor* Descriptor, FFragmentRegistrationContext& Context)
{
	using namespace UE::Net;
	static_assert(TFastArrayTypeHelper<FastArrayType>::HasValidFastArrayItemType(), "Invalid FastArrayItemType detected. Make sure that FastArraySerializer has a single replicated dynamic array");

	if constexpr (TIsDerivedFrom<FastArrayType, FIrisFastArraySerializer>::IsDerived)
	{
		if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsNativeFastArrayReplicationState))
		{
			typedef TNativeFastArrayReplicationFragment<typename TFastArrayTypeHelper<FastArrayType>::FastArrayItemType, FastArrayType, FastArrayPollingPolicies::FNeedPollingPolicy> FFragmentType;
			if (FFragmentType* Fragment = new FFragmentType(Context.GetFragmentTraits(), Owner, Descriptor))
			{
				Fragment->Register(Context, EReplicationFragmentTraits::DeleteWithInstanceProtocol);
				return Fragment;
			}
			return nullptr;
		}
	}

	typedef TFastArrayReplicationFragment<typename TFastArrayTypeHelper<FastArrayType>::FastArrayItemType, FastArrayType> FFragmentType;
	if (FFragmentType* Fragment = new FFragmentType(Context.GetFragmentTraits(), Owner, Descriptor))
	{
		Fragment->Register(Context, EReplicationFragmentTraits::DeleteWithInstanceProtocol);
		return Fragment;
	}
	return nullptr;
}

}} // End of namespaces


