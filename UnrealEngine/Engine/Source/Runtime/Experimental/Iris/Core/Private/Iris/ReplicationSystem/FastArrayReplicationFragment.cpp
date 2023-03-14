// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/FastArrayReplicationFragment.h"
#include "CoreTypes.h"
#include "Iris/Core/IrisLog.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationState/InternalPropertyReplicationState.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializers.h"

namespace UE::Net::Private
{

const FReplicationStateDescriptor* FFastArrayReplicationFragmentBase::GetFastArraySerializerPropertyDescriptor() const
{
	const FReplicationStateMemberSerializerDescriptor& FastArrayMemberSerializerDescriptor = ReplicationStateDescriptor->MemberSerializerDescriptors[0];
	const FStructNetSerializerConfig* FastArrayStructSerializerConfig = static_cast<const FStructNetSerializerConfig*>(FastArrayMemberSerializerDescriptor.SerializerConfig);

	return FastArrayStructSerializerConfig->StateDescriptor;
}

FFastArrayReplicationFragmentBase::FFastArrayReplicationFragmentBase(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor)
: FReplicationFragment(InTraits)
, ReplicationStateDescriptor(InDescriptor)
, Owner(InOwner)
{
	// Verify that class descriptor is of expected type
	{
		check(EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::IsFastArrayReplicationState));
		check(!EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::IsNativeFastArrayReplicationState));
	}

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
				check(false);
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

	// Propagate object reference.
	if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasObjectReference))
	{
		Traits |= EReplicationFragmentTraits::HasObjectReference;
	}

	Traits |= EReplicationFragmentTraits::HasPropertyReplicationState;

	// Look up the descriptor of the struct
	const FReplicationStateDescriptor* FastArrayStructDescriptor = GetFastArraySerializerPropertyDescriptor();

	// And we are looking for the offset of the first and only member of the struct which is the relative offest to the FastArray
	WrappedArrayOffsetRelativeFastArraySerializerProperty = FastArrayStructDescriptor->MemberDescriptors[0].ExternalMemberOffset;

#if WITH_PUSH_MODEL
	if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasPushBasedDirtiness))
	{
		FFastArraySerializer* FastArraySerializer = reinterpret_cast<FFastArraySerializer*>(reinterpret_cast<uint8*>(InOwner) + InDescriptor->MemberProperties[0]->GetOffset_ForGC());
		FastArraySerializer->CachePushModelState(InOwner, InDescriptor->MemberProperties[0]->RepIndex);
		Traits |= EReplicationFragmentTraits::HasPushBasedDirtiness;
	}
#endif
}

const FReplicationStateDescriptor* FFastArrayReplicationFragmentBase::GetArrayElementDescriptor() const
{
	const FReplicationStateDescriptor* FastArrayDescriptor = ReplicationStateDescriptor.GetReference();
	const FReplicationStateDescriptor* FastArrayStructDescriptor = static_cast<const FStructNetSerializerConfig*>(FastArrayDescriptor->MemberSerializerDescriptors[0].SerializerConfig)->StateDescriptor;
	const FReplicationStateDescriptor* ElementStateDescriptor = static_cast<const FArrayPropertyNetSerializerConfig*>(FastArrayStructDescriptor->MemberSerializerDescriptors[0].SerializerConfig)->StateDescriptor;
	const FReplicationStateMemberSerializerDescriptor& ElementSerializerDescriptor = ElementStateDescriptor->MemberSerializerDescriptors[0];
	
	return static_cast<const FStructNetSerializerConfig*>(ElementSerializerDescriptor.SerializerConfig)->StateDescriptor;
}

void FFastArrayReplicationFragmentBase::InternalCopyArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	InternalCopyStructProperty(ArrayElementDescriptor, Dst, Src);
}

bool FFastArrayReplicationFragmentBase::InternalCompareArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	return InternalCompareStructProperty(ArrayElementDescriptor, Dst, Src);
}

void FFastArrayReplicationFragmentBase::Register(FFragmentRegistrationContext& Context, EReplicationFragmentTraits InTraits)
{
	this->Traits |= InTraits;
	Context.RegisterReplicationFragment(this, ReplicationStateDescriptor.GetReference(), SrcReplicationState ? SrcReplicationState->GetStateBuffer() : nullptr);
}

void FFastArrayReplicationFragmentBase::CollectOwner(FReplicationStateOwnerCollector* Owners) const
{
	Owners->AddOwner(Owner);
}

void FFastArrayReplicationFragmentBase::CallRepNotifies(FReplicationStateApplyContext& Context)
{
	IRIS_PROFILER_SCOPE(PropertyReplicationFragment_InvokeRepNotifies);

	const FPropertyReplicationState ReceivedState(Context.Descriptor, Context.StateBufferData.ExternalStateBuffer);

	if (!Context.bIsInit)
	{
		ReceivedState.CallRepNotifies(Owner, nullptr, Context.bIsInit);
	}
	else
	{
		// As our default initial states is always treated as all dirty we need to compare against the default before applying initial repnotifies
		const FPropertyReplicationState DefaultState(Context.Descriptor);

		ReceivedState.CallRepNotifies(Owner, &DefaultState, Context.bIsInit);
	}
}

void FFastArrayReplicationFragmentBase::ReplicatedStateToString(FStringBuilderBase& StringBuilder, FReplicationStateApplyContext& Context, EReplicationStateToStringFlags Flags) const
{
	const FPropertyReplicationState ReceivedState(Context.Descriptor, Context.StateBufferData.ExternalStateBuffer);

	const bool bIncludeAll = EnumHasAnyFlags(Flags, EReplicationStateToStringFlags::OnlyIncludeDirtyMembers) == false;
	ReceivedState.ToString(StringBuilder, bIncludeAll);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NativeFastArrayReplicationFragment
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const FReplicationStateDescriptor* FNativeFastArrayReplicationFragmentBase::GetFastArraySerializerPropertyDescriptor() const
{
	const FReplicationStateMemberSerializerDescriptor& FastArrayMemberSerializerDescriptor = ReplicationStateDescriptor->MemberSerializerDescriptors[0];
	const FStructNetSerializerConfig* FastArrayStructSerializerConfig = static_cast<const FStructNetSerializerConfig*>(FastArrayMemberSerializerDescriptor.SerializerConfig);

	return FastArrayStructSerializerConfig->StateDescriptor;
}

FNativeFastArrayReplicationFragmentBase::FNativeFastArrayReplicationFragmentBase(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor)
: FReplicationFragment(InTraits)
, ReplicationStateDescriptor(InDescriptor)
, Owner(InOwner)
{
	// Verify that class descriptor is of expected type
	{
		check(EnumHasAllFlags(InDescriptor->Traits, EReplicationStateTraits::IsFastArrayReplicationState | EReplicationStateTraits::IsNativeFastArrayReplicationState));
	}

	// We do not create temporary states
	Traits |= EReplicationFragmentTraits::HasPersistentTargetStateBuffer;
	
	if (EnumHasAnyFlags(InTraits, EReplicationFragmentTraits::CanReceive))
	{
		if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasRepNotifies))
		{
			if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::KeepPreviousState))
			{
				check(false);
			}

			Traits |= EReplicationFragmentTraits::HasRepNotifies;
		}

		// For now we always expect pre/post operations for legacy states
		Traits |= EReplicationFragmentTraits::NeedsLegacyCallbacks;
	}
	
	// Propagate object reference.
	if (EnumHasAnyFlags(InDescriptor->Traits, EReplicationStateTraits::HasObjectReference))
	{
		Traits |= EReplicationFragmentTraits::HasObjectReference;
	}

	// Look up the descriptor of the struct
	const FReplicationStateDescriptor* FastArrayStructDescriptor = GetFastArraySerializerPropertyDescriptor();

	// And we are looking for the offset of the first and only member of the struct which is the relative offest to the FastArray
	WrappedArrayOffsetRelativeFastArraySerializerProperty = FastArrayStructDescriptor->MemberDescriptors[0].ExternalMemberOffset;
}

const FReplicationStateDescriptor* FNativeFastArrayReplicationFragmentBase::GetArrayElementDescriptor() const
{
	const FReplicationStateDescriptor* FastArrayDescriptor = ReplicationStateDescriptor.GetReference();
	const FReplicationStateDescriptor* FastArrayStructDescriptor = static_cast<const FStructNetSerializerConfig*>(FastArrayDescriptor->MemberSerializerDescriptors[0].SerializerConfig)->StateDescriptor;
	const FReplicationStateDescriptor* ElementStateDescriptor = static_cast<const FArrayPropertyNetSerializerConfig*>(FastArrayStructDescriptor->MemberSerializerDescriptors[0].SerializerConfig)->StateDescriptor;
	const FReplicationStateMemberSerializerDescriptor& ElementSerializerDescriptor = ElementStateDescriptor->MemberSerializerDescriptors[0];
	
	return static_cast<const FStructNetSerializerConfig*>(ElementSerializerDescriptor.SerializerConfig)->StateDescriptor;
}

void FNativeFastArrayReplicationFragmentBase::InternalDequantizeFastArray(FNetSerializationContext& Context, uint8* RESTRICT DstExternalBuffer, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	const uint32 MemberIndex = 0u;

	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->MemberSerializerDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	check(MemberIndex < MemberCount);

	const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIndex];
	const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = MemberSerializerDescriptors[MemberIndex];

	FNetDequantizeArgs Args;
	Args.Version = 0;
	Args.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
	Args.Target = reinterpret_cast<NetSerializerValuePointer>(DstExternalBuffer + MemberDescriptor.ExternalMemberOffset);
	Args.Source = reinterpret_cast<NetSerializerValuePointer>(SrcInternalBuffer + MemberDescriptor.InternalMemberOffset);	

	MemberSerializerDescriptor.Serializer->Dequantize(Context, Args);
}

void FNativeFastArrayReplicationFragmentBase::ToString(FStringBuilderBase& StringBuilder, const uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor)
{
	const FProperty** MemberProperties = Descriptor->MemberProperties;
	const uint32 MemberCount = Descriptor->MemberCount;

	StringBuilder.Appendf(TEXT("FNativeFastArrayReplicationState %s\n"), Descriptor->DebugName->Name);

	FString TempString;
	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FProperty* Property = MemberProperties[MemberIt];

		Property->ExportTextItem_Direct(TempString, StateBuffer + Descriptor->MemberDescriptors[MemberIt].ExternalMemberOffset, nullptr, nullptr, PPF_SimpleObjectText);
		StringBuilder.Appendf(TEXT("%u - %s : %s\n"), MemberIt, *Property->GetName(), ToCStr(TempString));
		TempString.Reset();
	}
}

void FNativeFastArrayReplicationFragmentBase::CollectOwner(FReplicationStateOwnerCollector* Owners) const
{
	Owners->AddOwner(Owner);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FFastArrayReplicationFragmentHelper
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FFastArrayReplicationFragmentHelper::InternalCopyArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	InternalCopyStructProperty(ArrayElementDescriptor, Dst, Src);
}

bool FFastArrayReplicationFragmentHelper::InternalCompareArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src)
{
	return InternalCompareStructProperty(ArrayElementDescriptor, Dst, Src);
}

TRefCountPtr<const FReplicationStateDescriptor> FFastArrayReplicationFragmentHelper::GetOrCreateDescriptorForFastArrayProperty(UObject* Object, FFragmentRegistrationContext& Context, int32 RepIndex)
{
	UClass* ObjectClass = Object->GetClass();

	// If we already have created descriptors for the class this will be a lookup in the registry, if not we will explicitly create a state for the 
	// given property only and do not register it
	FReplicationStateDescriptorBuilder::FParameters BuilderParameters;
	BuilderParameters.SinglePropertyIndex = RepIndex;
	BuilderParameters.DefaultStateSource = Object->GetArchetype();
	BuilderParameters.DescriptorRegistry = Context.GetReplicationStateRegistry();
	
	FReplicationStateDescriptorBuilder::FResult Result;
	FReplicationStateDescriptorBuilder::CreateDescriptorsForClass(Result, ObjectClass, BuilderParameters);

	// Find the descriptor for the fast array
	for (auto& Desc : Result)
	{
		if (EnumHasAnyFlags(Desc->Traits, EReplicationStateTraits::IsFastArrayReplicationState))
		{
			if (Desc->MemberProperties[0]->RepIndex == RepIndex)
			{
				return Desc;
			}
		}
	}

	return nullptr;
}

}
