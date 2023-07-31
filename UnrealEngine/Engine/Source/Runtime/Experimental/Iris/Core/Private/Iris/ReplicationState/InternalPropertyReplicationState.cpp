// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationState/InternalPropertyReplicationState.h"
#include "CoreTypes.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationState/InternalReplicationStateDescriptorUtils.h"
#include "Iris/Serialization/NetSerializers.h"
#include "UObject/UnrealType.h"

namespace UE::Net::Private
{

void InitReplicationStateInternals(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	// Init internal data
	new (StateBuffer) FReplicationStateHeader();

	// init dirty state tracking
	FNetBitArrayView DirtyStates = GetMemberChangeMask(StateBuffer, Descriptor);
	DirtyStates.Reset();

	// Init optional conditionals
	if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals))
	{
		FNetBitArrayView ConditionalChangeMask = GetMemberConditionalChangeMask(StateBuffer, Descriptor);
		ConditionalChangeMask.SetAllBits();
	}
}

void CopyPropertyReplicationStateInternals(uint8* RESTRICT DstStateBuffer, uint8* RESTRICT SrcStateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	FNetBitArrayView DstChangeMask = GetMemberChangeMask(DstStateBuffer, Descriptor);
	FNetBitArrayView SrcChangeMask = GetMemberChangeMask(SrcStateBuffer, Descriptor);

	DstChangeMask.Copy(SrcChangeMask);

	// Copy optional conditional changemask
	if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals))
	{
		FNetBitArrayView DstConditionalChangeMask = GetMemberConditionalChangeMask(DstStateBuffer, Descriptor);
		FNetBitArrayView SrcConditionalChangeMask = GetMemberConditionalChangeMask(SrcStateBuffer, Descriptor);

		DstConditionalChangeMask.Copy(SrcConditionalChangeMask);
	}
}

void ConstructPropertyReplicationState(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	check(IsAligned(StateBuffer, Descriptor->ExternalAlignment));
	
	InitReplicationStateInternals(StateBuffer, Descriptor);

	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FProperty** MemberProperties = Descriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
		
		// InitializeValue operates on the entire static array so make sure not to call it other than for the first element.
		if (MemberPropertyDescriptor.ArrayIndex == 0)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
			const FProperty* Property = MemberProperties[MemberIt];
			Property->InitializeValue(StateBuffer + MemberDescriptor.ExternalMemberOffset);
		}
	}
}

void DestructPropertyReplicationState(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	check(IsAligned(StateBuffer, Descriptor->ExternalAlignment));
	if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsSourceTriviallyDestructible))
	{
		return;
	}

	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FProperty** MemberProperties = Descriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
		
		// DestroyValue operates on the entire static array so make sure not to call it other than for the first element.
		if (MemberPropertyDescriptor.ArrayIndex == 0)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
			const FProperty* Property = MemberProperties[MemberIt];
			Property->DestroyValue(StateBuffer + MemberDescriptor.ExternalMemberOffset);
		}
	}
}

void CopyPropertyReplicationState(uint8* RESTRICT DstStateBuffer, uint8* RESTRICT SrcStateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	check(IsAligned(DstStateBuffer, Descriptor->ExternalAlignment) && IsAligned(SrcStateBuffer, Descriptor->ExternalAlignment));

	// Copy changemasks
	CopyPropertyReplicationStateInternals(DstStateBuffer, SrcStateBuffer, Descriptor);

	// copy statedata
	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FProperty** MemberProperties = Descriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
		if (MemberPropertyDescriptor.ArrayIndex == 0)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
			const FProperty* Property = MemberProperties[MemberIt];
			Property->CopyCompleteValue(DstStateBuffer + MemberDescriptor.ExternalMemberOffset, SrcStateBuffer + MemberDescriptor.ExternalMemberOffset);
		}
	}
}

/**
 * Structs and Arrays need to be handled carefully. Our intermediate representation need 
 * to match the actual native representation perfectly. We are careful not
 * to copy anything that isn't replicated and not consider a struct dirty
 * in case a non-replicated member was modified. Rep notifies that require
 * the previous value as parameter will only get the replicated values
 * updated- non-replicated members will be the same as the default state.
 */
bool InternalCompareMember(const FReplicationStateDescriptor* Descriptor, uint32 MemberIndex, const void* RESTRICT ValueA, const void* RESTRICT ValueB)
{
	const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = Descriptor->MemberSerializerDescriptors[MemberIndex];
	if (IsUsingStructNetSerializer(MemberSerializerDescriptor))
	{
		const FStructNetSerializerConfig* StructConfig = static_cast<const FStructNetSerializerConfig*>(MemberSerializerDescriptor.SerializerConfig);
		const FReplicationStateDescriptor* StructDescriptor = StructConfig->StateDescriptor;
		if (!EnumHasAnyFlags(StructDescriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
		{
			const FReplicationStateMemberDescriptor* MemberDescriptors = StructDescriptor->MemberDescriptors;
			const FProperty** MemberProperties = StructDescriptor->MemberProperties;
			const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = StructDescriptor->MemberPropertyDescriptors;

			for (uint32 StructMemberIt = 0, StructMemberEndIt = StructDescriptor->MemberCount; StructMemberIt != StructMemberEndIt; ++StructMemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[StructMemberIt];
				const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[StructMemberIt];
				const FProperty* MemberProperty = MemberProperties[StructMemberIt];
				const SIZE_T MemberOffset = MemberProperty->GetOffset_ForGC() + MemberProperty->ElementSize*MemberPropertyDescriptor.ArrayIndex;
				if (!InternalCompareMember(StructDescriptor, StructMemberIt, static_cast<const uint8*>(ValueA) + MemberOffset, static_cast<const uint8*>(ValueB) + MemberOffset))
				{
					return false;
				}
			}

			return true;
		}
	}
	else if (IsUsingArrayPropertyNetSerializer(MemberSerializerDescriptor))
	{
		const FArrayPropertyNetSerializerConfig* ArrayConfig = static_cast<const FArrayPropertyNetSerializerConfig*>(MemberSerializerDescriptor.SerializerConfig);
		const FReplicationStateDescriptor* StructDescriptor = ArrayConfig->StateDescriptor;
		if (!EnumHasAnyFlags(StructDescriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
		{
			FNetSerializationContext Context;
			FNetIsEqualArgs Args;
			Args.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
			Args.Source0 = NetSerializerValuePointer(ValueA);
			Args.Source1 = NetSerializerValuePointer(ValueB);
			Args.bStateIsQuantized = false;

			return MemberSerializerDescriptor.Serializer->IsEqual(Context, Args);
		}
	}

	// Default handling for all properties except for structs and arrays that have some non-replicated members
	const FProperty* Property = Descriptor->MemberProperties[MemberIndex];
	return Property->Identical(ValueA, ValueB);
}

bool InternalCompareStructProperty(const FReplicationStateDescriptor* StructDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	const FReplicationStateMemberDescriptor* MemberDescriptors = StructDescriptor->MemberDescriptors;
	const FProperty** MemberProperties = StructDescriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = StructDescriptor->MemberPropertyDescriptors;

	for (uint32 StructMemberIt = 0, StructMemberEndIt = StructDescriptor->MemberCount; StructMemberIt != StructMemberEndIt; ++StructMemberIt)
	{
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[StructMemberIt];
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[StructMemberIt];
		const FProperty* MemberProperty = MemberProperties[StructMemberIt];
		const SIZE_T MemberOffset = MemberProperty->GetOffset_ForGC() + MemberProperty->ElementSize*MemberPropertyDescriptor.ArrayIndex;
		if (!InternalCompareMember(StructDescriptor, StructMemberIt, static_cast<uint8*>(Dst) + MemberOffset, static_cast<const uint8*>(Src) + MemberOffset))
		{
			return false;
		}
	}

	return true;
}

void InternalCopyStructProperty(const FReplicationStateDescriptor* StructDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	const FReplicationStateMemberDescriptor* MemberDescriptors = StructDescriptor->MemberDescriptors;
	const FProperty** MemberProperties = StructDescriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = StructDescriptor->MemberPropertyDescriptors;

	for (uint32 StructMemberIt = 0, StructMemberEndIt = StructDescriptor->MemberCount; StructMemberIt != StructMemberEndIt; ++StructMemberIt)
	{
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[StructMemberIt];
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[StructMemberIt];
		const FProperty* MemberProperty = MemberProperties[StructMemberIt];
		const SIZE_T MemberOffset = MemberProperty->GetOffset_ForGC() + MemberProperty->ElementSize*MemberPropertyDescriptor.ArrayIndex;
		InternalCopyPropertyValue(StructDescriptor, StructMemberIt, static_cast<uint8*>(Dst) + MemberOffset, static_cast<const uint8*>(Src) + MemberOffset);
	}
}

void InternalCopyPropertyValue(const FReplicationStateDescriptor* Descriptor, uint32 MemberIndex, void* RESTRICT Dst, const void* RESTRICT Src)
{
	const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = Descriptor->MemberSerializerDescriptors[MemberIndex];
	if (IsUsingStructNetSerializer(MemberSerializerDescriptor))
	{
		const FStructNetSerializerConfig* StructConfig = static_cast<const FStructNetSerializerConfig*>(MemberSerializerDescriptor.SerializerConfig);
		const FReplicationStateDescriptor* StructDescriptor = StructConfig->StateDescriptor;
		if (!EnumHasAnyFlags(StructDescriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
		{
			InternalCopyStructProperty(StructDescriptor, Dst, Src);
			return;
		}
	}
	else if (IsUsingArrayPropertyNetSerializer(MemberSerializerDescriptor))
	{
		const FArrayPropertyNetSerializerConfig* ArrayConfig = static_cast<const FArrayPropertyNetSerializerConfig*>(MemberSerializerDescriptor.SerializerConfig);
		const FReplicationStateDescriptor* ElementStateDescriptor = ArrayConfig->StateDescriptor;
		const FReplicationStateMemberSerializerDescriptor& ElementSerializerDescriptor = ElementStateDescriptor->MemberSerializerDescriptors[0];

		if (IsUsingStructNetSerializer(ElementSerializerDescriptor) && !EnumHasAnyFlags(ElementStateDescriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
		{
			const FReplicationStateDescriptor* StructDescriptor = static_cast<const FStructNetSerializerConfig*>(ElementSerializerDescriptor.SerializerConfig)->StateDescriptor;
	
			// Need to explicitly iterate over array members to be able to only copy data that we should copy
			FScriptArrayHelper ScriptArrayHelperSrc(ArrayConfig->Property.Get(), reinterpret_cast<const void*>(Src));
			FScriptArrayHelper ScriptArrayHelperDst(ArrayConfig->Property.Get(), reinterpret_cast<void*>(Dst));

			// First we must resize target to match size of source data
			const uint32 ElementCount = ScriptArrayHelperSrc.Num();
			ScriptArrayHelperDst.Resize(ElementCount);
		
			const FReplicationStateMemberDescriptor* MemberDescriptors = StructDescriptor->MemberDescriptors;
			const FProperty** MemberProperties = StructDescriptor->MemberProperties;
			const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = StructDescriptor->MemberPropertyDescriptors;

			// Iterate over array entries and copy the statedata using internal data
			for (uint32 ElementIt = 0, ElementEndIt = ElementCount; ElementIt < ElementEndIt; ++ElementIt)
			{
				const uint8*  ArraySrc = ScriptArrayHelperSrc.GetRawPtr(ElementIt);
				uint8* ArrayDst = ScriptArrayHelperDst.GetRawPtr(ElementIt);

				InternalCopyStructProperty(StructDescriptor, ArrayDst, ArraySrc);
			}
			return;
		}
	}

	// Default handling for all properties except for structs and arrays that have some non-replicated members
	const FProperty* Property = Descriptor->MemberProperties[MemberIndex];
	Property->CopySingleValue(Dst, Src);
}

}
