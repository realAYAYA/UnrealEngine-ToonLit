// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationOperationsInternal.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ChangeMaskCache.h"
#include "Iris/ReplicationSystem/ChangeMaskUtil.h"
#include "Iris/ReplicationSystem/NetHandleManager.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationState/InternalReplicationStateDescriptorUtils.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Math/NumericLimits.h"

namespace UE::Net::Private
{

void FReplicationInstanceOperationsInternal::BindInstanceProtocol(uint32 ReplicationSystemId, uint32 InternalReplicationIndex, FReplicationInstanceProtocol* InstanceProtocol, const FReplicationProtocol* Protocol)
{
	FReplicationInstanceProtocol::FFragmentData* FragmentData = InstanceProtocol->FragmentData;
	const FReplicationStateDescriptor** Descriptors = Protocol->ReplicationStateDescriptors;

	const uint32 FragmentCount = InstanceProtocol->FragmentCount;
	for (uint32 It = 0; It < FragmentCount; ++It)
	{
		if (FragmentData[It].ExternalSrcBuffer)
		{
			UE::Net::FReplicationStateHeader& ReplicationStateHeader = UE::Net::Private::GetReplicationStateHeader(FragmentData[It].ExternalSrcBuffer, Descriptors[It]);

			// should be initialized to 0 when we bind it
			check(ReplicationStateHeader.IsBound() == false || InternalReplicationIndex == 0U);

			UE::Net::Private::FReplicationStateHeaderAccessor::SetReplicationIndex(ReplicationStateHeader, InternalReplicationIndex, ReplicationSystemId);
		}
	}

	InstanceProtocol->InstanceTraits = InternalReplicationIndex != 0U ? InstanceProtocol->InstanceTraits | EReplicationInstanceProtocolTraits::IsBound : InstanceProtocol->InstanceTraits &= ~EReplicationInstanceProtocolTraits::IsBound;
}

void FReplicationInstanceOperationsInternal::UnbindInstanceProtocol(FReplicationInstanceProtocol* InstanceProtocol, const FReplicationProtocol* Protocol)
{ 
	if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::IsBound))
	{
		BindInstanceProtocol(0, 0, InstanceProtocol, Protocol);
	}
}

uint32 FReplicationInstanceOperationsInternal::CopyObjectStateData(FNetBitStreamWriter& ChangeMaskWriter, FChangeMaskCache& Cache, FNetHandleManager& NetHandleManager, FNetSerializationContext& SerializationContext, uint32 InternalIndex)
{
	if (NetHandleManager.IsScopableIndex(InternalIndex))
	{
		const FNetHandleManager::FReplicatedObjectData& Object = NetHandleManager.GetReplicatedObjectDataNoCheck(InternalIndex);

		// We cannot copy state data for zero sized objects or objects that no longer has an instance protocol.
		if (Object.InstanceProtocol && Object.Protocol->InternalTotalSize > 0U)
		{
			bool bShouldPropagateChangedStates = Object.bShouldPropagateChangedStates;

			// Copy dirty state
			{
				// Add entry to cache
				FChangeMaskCache::FCachedInfo& Info = Cache.AddChangeMaskForObject(InternalIndex, Object.Protocol->ChangeMaskBitCount);
				const uint32 ChangeMaskByteCount = FNetBitArrayView::CalculateRequiredWordCount(Object.Protocol->ChangeMaskBitCount) * 4;
				ChangeMaskStorageType* ChangeMaskData = Cache.GetChangeMaskStorage(Info);
				ChangeMaskWriter.InitBytes(ChangeMaskData, ChangeMaskByteCount);

				//UE_LOG(LogIris, Log, TEXT("Copying state data for ( InternalIndex: %u ) NethHandleIndex: %u"), InternalIndex, Object.Handle.GetIndex());
				FReplicationInstanceOperations::CopyAndQuantize(SerializationContext, NetHandleManager.GetReplicatedObjectStateBufferNoCheck(InternalIndex), &ChangeMaskWriter, Object.InstanceProtocol, Object.Protocol);

				Info.bHasDirtyChangeMask = MakeNetBitArrayView(ChangeMaskData, ChangeMaskByteCount * 8U).FindFirstOne() != FNetBitArrayView::InvalidIndex;
			}

			// Mark subobject owner as dirty if this is a subobject
			if (const uint32 SubObjectOwnerIndex = Object.SubObjectRootIndex)
			{
				const bool bIsOwnerScopable = NetHandleManager.IsScopableIndex(SubObjectOwnerIndex);
				// Dependent objects should not ensure if the owner isn't scopable.
				ensureMsgf(bIsOwnerScopable || Object.IsDependentObject(), TEXT("SubObject ( InternaIndex: %u ) with NetHandleIndex: %u is trying to dirty parent ( InternalIndex: %u ) not in scope."), InternalIndex, Object.Handle.GetId(), SubObjectOwnerIndex);
				if (bIsOwnerScopable)
				{
					// Do we want to control this separately for subobjects? Or should they respect the setting on the owner?
					// For now, we do and will not mark owner as dirty if owner should not propagate statechanges
					bShouldPropagateChangedStates = bShouldPropagateChangedStates && NetHandleManager.GetReplicatedObjectDataNoCheck(SubObjectOwnerIndex).bShouldPropagateChangedStates;
					if (bShouldPropagateChangedStates)
					{
						//UE_LOG(LogIris, Log, TEXT("Marking SubObjectOwner( InternalIndex: %u ) as dirty for ( InternalIndex: %u ) NethHandleIndex: %u"), InternalIndex, Object.Handle.GetIndex());
						Cache.AddSubObjectOwnerDirty(SubObjectOwnerIndex);
					}
				}
			}

			// If changed states should not be propagated, we must pop the last entry added to the cache
			if (!bShouldPropagateChangedStates)
			{
				Cache.PopLastEntry();
			}

			return 1U;
		}
		else if (const uint32 SubObjectOwnerIndex = Object.SubObjectRootIndex)
		{
			if (Object.bShouldPropagateChangedStates && ensure(NetHandleManager.IsScopableIndex(SubObjectOwnerIndex)))
			{	
				// Do we want to control this separately for subobjects? Or should they respect the setting on the owner?
				// For now, we do and will not mark owner as dirty if owner should not propagate statechanges
				if (NetHandleManager.GetReplicatedObjectDataNoCheck(SubObjectOwnerIndex).bShouldPropagateChangedStates)
				{
					//UE_LOG(LogIris, Log, TEXT("Marking SubObjectOwner( InternalIndex: %u ) as dirty for ( InternalIndex: %u ) NethHandleIndex: %u"), InternalIndex, Object.Handle.GetIndex());
					Cache.AddSubObjectOwnerDirty(SubObjectOwnerIndex);
				}
			}			
		}
	}
	else
	{
		// Deal with error, object not found
		UE_LOG(LogIris, Warning, TEXT("CopyObjectStateData called on object ( InternalIndex: %u ) not in scope"), InternalIndex);
	}

	return 0U;
}

void FReplicationStateOperationsInternal::CloneDynamicState(FNetSerializationContext& Context, uint8* RESTRICT DstInternalBuffer, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->MemberSerializerDescriptors;
	const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->MemberTraitsDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberTraitsDescriptor& MemberTraitsDescriptor = MemberTraitsDescriptors[MemberIt];
		if (!EnumHasAnyFlags(MemberTraitsDescriptor.Traits, EReplicationStateMemberTraits::HasDynamicState))
		{
			continue;
		}

		const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = MemberSerializerDescriptors[MemberIt];
		const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];

		FNetCloneDynamicStateArgs MemberArgs;
		MemberArgs.Version = 0;
		MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
		MemberArgs.Source = NetSerializerValuePointer(SrcInternalBuffer) + MemberDescriptor.InternalMemberOffset;
		MemberArgs.Target = NetSerializerValuePointer(DstInternalBuffer) + MemberDescriptor.InternalMemberOffset;

		Serializer->CloneDynamicState(Context, MemberArgs);
	}
}

void FReplicationStateOperationsInternal::CloneQuantizedState(FNetSerializationContext& Context, uint8* RESTRICT DstInternalBuffer, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	// Memcopy state storage
	FMemory::Memcpy(DstInternalBuffer, SrcInternalBuffer, Descriptor->InternalSize);

	// If no member has dynamic state then there's nothing for us to do
	if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
	{
		return;
	}

	// Clone dynamic state
	CloneDynamicState(Context, DstInternalBuffer, SrcInternalBuffer, Descriptor);
}

void FReplicationStateOperationsInternal::FreeDynamicState(FNetSerializationContext& Context, uint8* ObjectStateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->MemberSerializerDescriptors;
	const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->MemberTraitsDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberTraitsDescriptor& MemberTraitsDescriptor = MemberTraitsDescriptors[MemberIt];
		if (!EnumHasAnyFlags(MemberTraitsDescriptor.Traits, EReplicationStateMemberTraits::HasDynamicState))
		{
			continue;
		}

		const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = MemberSerializerDescriptors[MemberIt];
		const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];

		FNetFreeDynamicStateArgs MemberArgs;
		MemberArgs.Version = 0;
		MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
		MemberArgs.Source = NetSerializerValuePointer(ObjectStateBuffer) + MemberDescriptor.InternalMemberOffset;

		Serializer->FreeDynamicState(Context, MemberArgs);
	}
}

void FReplicationStateOperationsInternal::FreeDynamicState(uint8* ObjectStateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	FNetSerializationContext Context;
	FInternalNetSerializationContext InternalContext;
	Context.SetInternalContext(&InternalContext);

	FReplicationStateOperationsInternal::FreeDynamicState(Context, ObjectStateBuffer, Descriptor);
}

void FReplicationStateOperationsInternal::CollectReferences(FNetSerializationContext& Context, FNetReferenceCollector& Collector, const FNetSerializerChangeMaskParam& OuterChangeMaskInfo, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	const FReplicationStateMemberReferenceDescriptor* ReferenceDescriptors = Descriptor->MemberReferenceDescriptors;
	
	for (const FReplicationStateMemberReferenceDescriptor& MemberReferenceDescriptor : MakeArrayView(ReferenceDescriptors, Descriptor->ObjectReferenceCount))
	{
		// Direct reference located in the buffer, we just need the offset to the NetObjectReference
		if (MemberReferenceDescriptor.Info.ResolveType != FNetReferenceInfo::EResolveType::Invalid)
		{
			Collector.Add(MemberReferenceDescriptor.Info, *reinterpret_cast<const FNetObjectReference*>(SrcInternalBuffer + MemberReferenceDescriptor.Offset), OuterChangeMaskInfo);
		}
		else
		{
			// For dynamic arrays and serializers with custom references we need to find the actual MemberDescriptor so that we can invoke the CollectNetRefrences function
			const FReplicationStateMemberReferenceDescriptor* CurrentReferenceDescriptor = &MemberReferenceDescriptor;
			const FReplicationStateDescriptor* CurrentDescriptor = Descriptor;
			const uint8* RESTRICT CurrentInternalBuffer = SrcInternalBuffer;

			for (;;)
			{
				const uint32 MemberIndex = CurrentReferenceDescriptor->MemberIndex;
				const FReplicationStateMemberSerializerDescriptor* SerializerInfo = &CurrentDescriptor->MemberSerializerDescriptors[MemberIndex];
				const FReplicationStateMemberDescriptor& MemberDescriptor = CurrentDescriptor->MemberDescriptors[MemberIndex];	
				const uint16 InnerReferenceIndex = CurrentReferenceDescriptor->InnerReferenceIndex;

				const bool bIsMemberWithCustomReferences = InnerReferenceIndex == MAX_uint16;
				if (bIsMemberWithCustomReferences)
				{
					// Verify that this is a dynamic array, or a serializer with custom references otherwise something is seriously broken
					checkSlow(IsUsingArrayPropertyNetSerializer(*SerializerInfo) || EnumHasAnyFlags(SerializerInfo->Serializer->Traits, ENetSerializerTraits::HasCustomNetReference));

					FNetCollectReferencesArgs Args = {};
					Args.Source = reinterpret_cast<NetSerializerValuePointer>(CurrentInternalBuffer + MemberDescriptor.InternalMemberOffset);
					Args.NetSerializerConfig = SerializerInfo->SerializerConfig;
					Args.ChangeMaskInfo = OuterChangeMaskInfo;
					Args.Collector = reinterpret_cast<NetSerializerValuePointer>(&Collector);
					Args.Version = 0U;
					
					SerializerInfo->Serializer->CollectNetReferences(Context, Args);
					break;
				}

				// The reference was to a nested inner reference
				const FStructNetSerializerConfig* StructConfig = static_cast<const FStructNetSerializerConfig*>(SerializerInfo->SerializerConfig);
				CurrentDescriptor = StructConfig->StateDescriptor;
				CurrentReferenceDescriptor = &StructConfig->StateDescriptor->MemberReferenceDescriptors[InnerReferenceIndex];
				CurrentInternalBuffer = CurrentInternalBuffer + MemberDescriptor.InternalMemberOffset;
			}
		}
	}
}

void FReplicationStateOperationsInternal::CollectReferencesWithMask(FNetSerializationContext& Context, FNetReferenceCollector& Collector, const uint32 ChangeMaskOffset, const uint8* RESTRICT SrcInternalBuffer,  const FReplicationStateDescriptor* Descriptor)
{
	const FReplicationStateMemberReferenceDescriptor* ReferenceDescriptors = Descriptor->MemberReferenceDescriptors;
	const FReplicationStateMemberChangeMaskDescriptor* ChangeMaskDescriptors = Descriptor->MemberChangeMaskDescriptors;
	const FNetBitArrayView* ChangeMask = Context.GetChangeMask();

	for (const FReplicationStateMemberReferenceDescriptor& MemberReferenceDescriptor : MakeArrayView(ReferenceDescriptors, Descriptor->ObjectReferenceCount))
	{
		checkSlow((ChangeMaskOffset + ChangeMaskDescriptors[MemberReferenceDescriptor.MemberIndex].BitOffset) < MAX_uint16);

		const FNetSerializerChangeMaskParam LocalChangeMaskInfo = FNetSerializerChangeMaskParam( { (uint16)(ChangeMaskOffset + ChangeMaskDescriptors[MemberReferenceDescriptor.MemberIndex].BitOffset), ChangeMaskDescriptors[MemberReferenceDescriptor.MemberIndex].BitCount } );
		if (ChangeMask && !ChangeMask->IsAnyBitSet(LocalChangeMaskInfo.BitOffset, LocalChangeMaskInfo.BitCount))
		{
			continue;
		}
	
		// Direct reference located in the buffer, we just need the offset to the NetObjectReference
		if (MemberReferenceDescriptor.Info.ResolveType != FNetReferenceInfo::EResolveType::Invalid)
		{
			Collector.Add(MemberReferenceDescriptor.Info, *reinterpret_cast<const FNetObjectReference*>(SrcInternalBuffer + MemberReferenceDescriptor.Offset), LocalChangeMaskInfo);
		}
		else
		{
			// For dynamic arrays and serializers with custom references we need to find the actual MemberDescriptor so that we can invoke the CollectNetRefrences function
			const FReplicationStateMemberReferenceDescriptor* CurrentReferenceDescriptor = &MemberReferenceDescriptor;
			const FReplicationStateDescriptor* CurrentDescriptor = Descriptor;
			const uint8* RESTRICT CurrentInternalBuffer = SrcInternalBuffer;

			for (;;)
			{
				const uint32 MemberIndex = CurrentReferenceDescriptor->MemberIndex;
				const FReplicationStateMemberSerializerDescriptor* SerializerInfo = &CurrentDescriptor->MemberSerializerDescriptors[MemberIndex];
				const FReplicationStateMemberDescriptor& MemberDescriptor = CurrentDescriptor->MemberDescriptors[MemberIndex];	
				const uint16 InnerReferenceIndex = CurrentReferenceDescriptor->InnerReferenceIndex;

				const bool bIsMemberWithCustomReferences = InnerReferenceIndex == MAX_uint16;
				if (bIsMemberWithCustomReferences)
				{
					// Verify that this is a dynamic array, or a serializer with custom references otherwise something is seriously broken
					checkSlow(IsUsingArrayPropertyNetSerializer(*SerializerInfo) || EnumHasAnyFlags(SerializerInfo->Serializer->Traits, ENetSerializerTraits::HasCustomNetReference));

					FNetCollectReferencesArgs Args = {};
					Args.Source = reinterpret_cast<NetSerializerValuePointer>(CurrentInternalBuffer + MemberDescriptor.InternalMemberOffset);
					Args.NetSerializerConfig = SerializerInfo->SerializerConfig;
					Args.ChangeMaskInfo = LocalChangeMaskInfo;
					Args.Collector = reinterpret_cast<NetSerializerValuePointer>(&Collector);
					Args.Version = 0U;
					
					SerializerInfo->Serializer->CollectNetReferences(Context, Args);
					break;
				}

				// The reference was to a nested inner reference
				const FStructNetSerializerConfig* StructConfig = static_cast<const FStructNetSerializerConfig*>(SerializerInfo->SerializerConfig);
				CurrentDescriptor = StructConfig->StateDescriptor;
				CurrentReferenceDescriptor = &StructConfig->StateDescriptor->MemberReferenceDescriptors[InnerReferenceIndex];
				CurrentInternalBuffer = CurrentInternalBuffer + MemberDescriptor.InternalMemberOffset;
			}
		}
	}
}

ENetObjectReferenceResolveResult FReplicationStateOperationsInternal::TryToResolveObjectReferences(FNetSerializationContext& Context, uint8* RESTRICT InternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	FNetSerializationContext LocalContext;
	FNetReferenceCollector Collector;

	if (Descriptor->IsInitState() || Descriptor->MemberChangeMaskDescriptors == nullptr)
	{
		const FNetSerializerChangeMaskParam InitStateChangeMaskInfo = { 0 };
		FReplicationStateOperationsInternal::CollectReferences(LocalContext, Collector, InitStateChangeMaskInfo, InternalBuffer, Descriptor);
	}
	else
	{
		FReplicationStateOperationsInternal::CollectReferencesWithMask(LocalContext, Collector, 0, InternalBuffer, Descriptor);
	}

	// Process references
	const FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	const FNetObjectResolveContext& ResolveContext = InternalContext->ResolveContext;
	FObjectReferenceCache& ObjectReferenceCache = *InternalContext->ObjectReferenceCache;

	ENetObjectReferenceResolveResult Result = ENetObjectReferenceResolveResult::None;
	for (const FNetReferenceCollector::FReferenceInfo& Info : MakeArrayView(Collector.GetCollectedReferences()))
	{
		UObject* ResolvedObject;
		Result |= ObjectReferenceCache.ResolveObjectReference(Info.Reference, ResolveContext, ResolvedObject);
	}

	return Result;
};

void FReplicationProtocolOperationsInternal::CloneDynamicState(FNetSerializationContext& Context, uint8* RESTRICT DstObjectStateBuffer, const uint8* RESTRICT SrcObjectStateBuffer, const FReplicationProtocol* Protocol)
{
	const FReplicationStateDescriptor** ReplicationStateDescriptors = Protocol->ReplicationStateDescriptors;
	uint8* CurrentInternalDstStateBuffer = DstObjectStateBuffer;
	const uint8* CurrentInternalSrcStateBuffer = SrcObjectStateBuffer;

	for (uint32 StateIt = 0, StateEndIt = Protocol->ReplicationStateCount; StateIt < StateEndIt; ++StateIt)
	{
		const FReplicationStateDescriptor* CurrentDescriptor = ReplicationStateDescriptors[StateIt];

		CurrentInternalDstStateBuffer = Align(CurrentInternalDstStateBuffer, CurrentDescriptor->InternalAlignment);
		CurrentInternalSrcStateBuffer = Align(CurrentInternalSrcStateBuffer, CurrentDescriptor->InternalAlignment);

		if (EnumHasAnyFlags(CurrentDescriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			FReplicationStateOperationsInternal::CloneDynamicState(Context, CurrentInternalDstStateBuffer, CurrentInternalSrcStateBuffer, CurrentDescriptor);
		}

		CurrentInternalDstStateBuffer += CurrentDescriptor->InternalSize;
		CurrentInternalSrcStateBuffer += CurrentDescriptor->InternalSize;
	}
}

void FReplicationProtocolOperationsInternal::FreeDynamicState(FNetSerializationContext& Context, uint8* RESTRICT ObjectStateBuffer, const FReplicationProtocol* Protocol)
{
	const FReplicationStateDescriptor** ReplicationStateDescriptors = Protocol->ReplicationStateDescriptors;
	uint8* CurrentInternalStateBuffer = ObjectStateBuffer;

	for (uint32 StateIt = 0, StateEndIt = Protocol->ReplicationStateCount; StateIt < StateEndIt; ++StateIt)
	{
		const FReplicationStateDescriptor* CurrentDescriptor = ReplicationStateDescriptors[StateIt];

		CurrentInternalStateBuffer = Align(CurrentInternalStateBuffer, CurrentDescriptor->InternalAlignment);
		
		if (EnumHasAnyFlags(CurrentDescriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			FReplicationStateOperationsInternal::FreeDynamicState(Context, CurrentInternalStateBuffer, CurrentDescriptor);
		}
		
		CurrentInternalStateBuffer += CurrentDescriptor->InternalSize;
	}
}

void FReplicationProtocolOperationsInternal::CollectReferences(FNetSerializationContext& Context, FNetReferenceCollector& Collector, const uint8* RESTRICT SrcInternalStateBuffer, const FReplicationProtocol* Protocol)
{
	const FNetSerializerChangeMaskParam InitStateChangeMaskInfo = {};
	const bool bIncludeInitStates = Context.IsInitState();
	const uint8* CurrentInternalStateBuffer = SrcInternalStateBuffer;
	uint32 CurrentChangeMaskBitOffset = 0;

	for (const FReplicationStateDescriptor* CurrentDescriptor : MakeArrayView(Protocol->ReplicationStateDescriptors, Protocol->ReplicationStateCount))
	{
		CurrentInternalStateBuffer = Align(CurrentInternalStateBuffer, CurrentDescriptor->InternalAlignment);
		
		if (CurrentDescriptor->HasObjectReference())
		{
			if (!CurrentDescriptor->IsInitState())
			{
				FReplicationStateOperationsInternal::CollectReferencesWithMask(Context, Collector, CurrentChangeMaskBitOffset, CurrentInternalStateBuffer, CurrentDescriptor);
			}
			else if (bIncludeInitStates)
			{
				FReplicationStateOperationsInternal::CollectReferences(Context, Collector, InitStateChangeMaskInfo, CurrentInternalStateBuffer, CurrentDescriptor);
			}
		}
		
		CurrentInternalStateBuffer += CurrentDescriptor->InternalSize;
		CurrentChangeMaskBitOffset += CurrentDescriptor->ChangeMaskBitCount;
	}
}

void FReplicationProtocolOperationsInternal::CloneQuantizedState(FNetSerializationContext& Context, uint8* RESTRICT DstObjectStateBuffer, const uint8* RESTRICT SrcObjectStateBuffer, const FReplicationProtocol* Protocol)
{
	FMemory::Memcpy(DstObjectStateBuffer, SrcObjectStateBuffer, Protocol->InternalTotalSize);
	if (EnumHasAnyFlags(Protocol->ProtocolTraits, EReplicationProtocolTraits::HasDynamicState))
	{
		FReplicationProtocolOperationsInternal::CloneDynamicState(Context, DstObjectStateBuffer, SrcObjectStateBuffer, Protocol);
		check(!Context.HasError());
	}
}

bool FReplicationProtocolOperationsInternal::IsEqualQuantizedState(FNetSerializationContext& Context, const uint8* RESTRICT State0, const uint8* RESTRICT State1, const FReplicationProtocol* Protocol)
{
	const bool bIncludeInitStates = Context.IsInitState();
	const uint8* CurrentState0 = State0;
	const uint8* CurrentState1 = State1;

	for (const FReplicationStateDescriptor* CurrentDescriptor : MakeArrayView(Protocol->ReplicationStateDescriptors, Protocol->ReplicationStateCount))
	{
		CurrentState0 = Align(CurrentState0, CurrentDescriptor->InternalAlignment);
		CurrentState1 = Align(CurrentState1, CurrentDescriptor->InternalAlignment);
		
		if (!CurrentDescriptor->IsInitState() || bIncludeInitStates)
		{
			const bool bStateIsEqual = FReplicationStateOperations::IsEqualQuantizedState(Context, CurrentState0, CurrentState1, CurrentDescriptor);
			if (!bStateIsEqual)
			{
				return false;
			}
		}

		CurrentState0 += CurrentDescriptor->InternalSize;
		CurrentState1 += CurrentDescriptor->InternalSize;
	}

	return true;
}

}
