// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationState/InternalPropertyReplicationState.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Core/NetBitArray.h"
#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Iris/Core/IrisLog.h"
#include "UObject/PropertyPortFlags.h"
#include "Containers/StringFwd.h"

namespace UE::Net
{

FPropertyReplicationState::FPropertyReplicationState(const FReplicationStateDescriptor* Descriptor, uint8* InStateBuffer)
: ReplicationStateDescriptor(Descriptor)
, StateBuffer(nullptr)
, bOwnState(0u)
{
	check(InStateBuffer);
	InjectState(Descriptor, InStateBuffer);
}

FPropertyReplicationState::FPropertyReplicationState(const FReplicationStateDescriptor* Descriptor)
: ReplicationStateDescriptor(Descriptor)
, StateBuffer(nullptr)
, bOwnState(1u)
{
	ConstructStateInternal();
}

/** Copy constructor, will not copy internal data */
FPropertyReplicationState::FPropertyReplicationState(const FPropertyReplicationState& Other)
: ReplicationStateDescriptor(Other.ReplicationStateDescriptor)
, StateBuffer(nullptr)
, bOwnState(1u)
{
	ConstructStateInternal();

	// Invoke assignment operator
	*this = Other;
}

FPropertyReplicationState& 
FPropertyReplicationState::operator=(const FPropertyReplicationState& Other)
{
	check(this != &Other && IsValid());
	check(ReplicationStateDescriptor.GetReference() == Other.ReplicationStateDescriptor.GetReference());

	if (!Private::IsReplicationStateBound(StateBuffer, ReplicationStateDescriptor.GetReference()))
	{
		Private::CopyPropertyReplicationState(StateBuffer, Other.StateBuffer, ReplicationStateDescriptor.GetReference());
	}
	else
	{
		Set(Other);
	}

	return *this;
}

FPropertyReplicationState::~FPropertyReplicationState()
{
	DestructStateInternal();
}

bool FPropertyReplicationState::IsValid() const
{
	return StateBuffer && ReplicationStateDescriptor;
}

bool FPropertyReplicationState::IsInitState() const
{
	return IsValid() && ReplicationStateDescriptor->IsInitState();
}

void FPropertyReplicationState::ConstructStateInternal()
{
	// allocate memory
	check(StateBuffer == nullptr);

	StateBuffer = (uint8*)FMemory::Malloc(ReplicationStateDescriptor->ExternalSize, ReplicationStateDescriptor->ExternalAlignment);
	// There can be properties in here that assume the memory is cleared and do nothing in InitializeValue
	FMemory::Memzero(StateBuffer, ReplicationStateDescriptor->ExternalSize);

	// Construct the state
	Private::ConstructPropertyReplicationState(StateBuffer, ReplicationStateDescriptor);
}

void FPropertyReplicationState::DestructStateInternal()
{
	// Destruct state and free memory if we own it
	if (bOwnState && StateBuffer)
	{
		Private::DestructPropertyReplicationState(StateBuffer, ReplicationStateDescriptor);
		FMemory::Free(StateBuffer);
		StateBuffer = nullptr;
	}
}

void FPropertyReplicationState::InjectState(const FReplicationStateDescriptor* Descriptor, uint8* InStateBuffer)
{
	// allocate memory
	check(StateBuffer == nullptr);
	check(InStateBuffer != nullptr);

	ReplicationStateDescriptor = Descriptor;
	StateBuffer = InStateBuffer;
}

void
FPropertyReplicationState::Set(const FPropertyReplicationState& Other)
{
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		uint8* DstStateBuffer = StateBuffer;
		uint8* SrcStateBuffer = Other.StateBuffer;

		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FProperty*const* Properties = Descriptor->MemberProperties;
		const uint32 MemberCount = Descriptor->MemberCount;

		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
			const FProperty* Property = Properties[MemberIt];

			uint8* DstValue = DstStateBuffer + MemberDescriptor.ExternalMemberOffset;
			uint8* SrcValue = SrcStateBuffer + MemberDescriptor.ExternalMemberOffset;
	
			// $IRIS TODO: Could use serializer IsEqual.
			if (!IsDirty(MemberIt) && IsCustomConditionEnabled(MemberIt) && !Property->Identical(SrcValue, DstValue))
			{
				MarkDirty(MemberIt);
			}

			// We need to use the internal copy as we do not want to run custom assign/copy constructors as we want a true copy of all replicated data
			Private::InternalCopyPropertyValue(Descriptor, MemberIt, DstValue, SrcValue);
		}
	}
}

void FPropertyReplicationState::SetPropertyValue(uint32 Index, const void* SrcValue)
{
	const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
	void* DstValue = StateBuffer + Descriptor->MemberDescriptors[Index].ExternalMemberOffset;

	if (!IsDirty(Index) && IsCustomConditionEnabled(Index) && !Private::InternalCompareMember(Descriptor, Index, SrcValue, DstValue))
	{
		MarkDirty(Index);
	}

	Private::InternalCopyPropertyValue(Descriptor, Index, DstValue, SrcValue);		
}

void FPropertyReplicationState::GetPropertyValue(uint32 Index, void* DstValue) const
{
	void* SrcValue = StateBuffer + ReplicationStateDescriptor->MemberDescriptors[Index].ExternalMemberOffset;
	Private::InternalCopyPropertyValue(ReplicationStateDescriptor, Index, DstValue, SrcValue);
}

void FPropertyReplicationState::MarkDirty(uint32 Index)
{
	const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
	FReplicationStateHeader& Header = Private::GetReplicationStateHeader(StateBuffer, Descriptor);

	if (IsInitState())
	{
		if (!Private::FReplicationStateHeaderAccessor::GetIsInitStateDirty(Header))
		{
			Private::FReplicationStateHeaderAccessor::MarkInitStateDirty(Header);
			if (Header.IsBound())
			{
				MarkNetObjectStateDirty(Header);
			}
		}
	}
	else
	{
		FNetBitArrayView MemberChangeMask = Private::GetMemberChangeMask(StateBuffer, Descriptor);
		const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskInfo = Descriptor->MemberChangeMaskDescriptors[Index];
		Private::MarkDirty(Header, MemberChangeMask, ChangeMaskInfo);
	}
}

bool FPropertyReplicationState::IsDirty(uint32 Index) const
{
	const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
	if (IsInitState())
	{
		const FReplicationStateHeader& Header = Private::GetReplicationStateHeader(StateBuffer, Descriptor);
		return Private::FReplicationStateHeaderAccessor::GetIsInitStateDirty(Header);
	}
	else
	{
		FNetBitArrayView MemberChangeMask = Private::GetMemberChangeMask(StateBuffer, Descriptor);
		const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskInfo = Descriptor->MemberChangeMaskDescriptors[Index];

		return MemberChangeMask.IsAnyBitSet(ChangeMaskInfo.BitOffset, ChangeMaskInfo.BitCount);
	}
}

void FPropertyReplicationState::PollPropertyReplicationState(const void* RESTRICT SrcStateData)
{
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		const uint8* SrcBuffer = reinterpret_cast<const uint8*>(SrcStateData);

		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
		const uint32 MemberCount = Descriptor->MemberCount;

		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
			const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
			const FProperty* Property = MemberProperties[MemberIt];

			//$TODO: make special version to avoid unnecessary overhead.
			SetPropertyValue(MemberIt, SrcBuffer + Property->GetOffset_ForGC() + Property->ElementSize*MemberPropertyDescriptor.ArrayIndex);
		}
	}
}

void FPropertyReplicationState::PushPropertyReplicationState(void* RESTRICT DstData, bool bInPushAll) const
{
	// $IRIS TODO: Rewrite this to iterate over change mask instead of iterating over all members and querying the mask
	// Note, we need to use a NetBitStreamReader and the changemask descriptor since each member might have different number of bits.
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		uint8* DstBuffer = reinterpret_cast<uint8*>(DstData);

		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
		const uint32 MemberCount = Descriptor->MemberCount;

		const bool bPushAll = bInPushAll || IsInitState();
		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			// Note: Currently not checking whether the condition is enabled or not. This is assuming few states would be affected by an early out here.
			// We need to mask off conditional state regardless at replication time to avoid replicating lost packets which contained state that has since been disabled.
			if (bPushAll || IsDirty(MemberIt))
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
				const FProperty* Property = MemberProperties[MemberIt];

				GetPropertyValue(MemberIt, DstBuffer + Property->GetOffset_ForGC() + Property->ElementSize*MemberPropertyDescriptor.ArrayIndex);
			}
		}
	}
}

void FPropertyReplicationState::PollObjectReferences(const void* RESTRICT SrcStateData)
{
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		const uint8* SrcBuffer = static_cast<const uint8*>(SrcStateData);

		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->MemberTraitsDescriptors;
		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
		const uint32 MemberCount = Descriptor->MemberCount;

		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			if (EnumHasAnyFlags(MemberTraitsDescriptors[MemberIt].Traits, EReplicationStateMemberTraits::HasObjectReference))
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
				const FProperty* Property = MemberProperties[MemberIt];

				SetPropertyValue(MemberIt, SrcBuffer + Property->GetOffset_ForGC() + Property->ElementSize*MemberPropertyDescriptor.ArrayIndex);
			}
		}
	}
}

void FPropertyReplicationState::CallRepNotifies(void* RESTRICT DstData, const FPropertyReplicationState* PreviousState, bool bIsInit) const
{
	// $IRIS TODO: Rewrite this to iterate over change mask instead of iterating over all members and querying the mask
	// Note, we need to use a NetBitStreamReader and the changemask descriptor since each member might have different number of bits.
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
		const uint32 MemberCount = Descriptor->MemberCount;

		const FProperty* LastPropertyWithRepNotify = nullptr;
		// Note: IsInitState indicates that the state itself is only ever will be applied at Init
		const bool bIsInitState = IsInitState();
		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
			const UFunction* RepNotifyFunction = MemberPropertyDescriptor.RepNotifyFunction;
			if (RepNotifyFunction && (bIsInitState || IsDirty(MemberIt)))
			{
				const FProperty* Property = MemberProperties[MemberIt];
				// If this is the same property we already processed it's yet another element in a C array.
				if (Property == LastPropertyWithRepNotify)
				{
					checkSlow(MemberPropertyDescriptor.ArrayIndex > 0);
					continue;
				}

				// For C arrays the RepNotify is for the entire array, not an individual element.
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const uint32 ExternalMemberOffset = MemberDescriptors[MemberIt].ExternalMemberOffset;
				
				UObject* Object = reinterpret_cast<UObject*>(DstData);
				const uint8* PrevValuePtr = PreviousState ? PreviousState->StateBuffer + ExternalMemberOffset : nullptr;
				const uint8* ValuePtr = StateBuffer + ExternalMemberOffset;

				// If it is the first time we apply state data, we only call repnotify if the value differs from the default
				if (!bIsInit || !Private::InternalCompareMember(Descriptor, MemberIt, ValuePtr, PrevValuePtr))
				{
					LastPropertyWithRepNotify = Property;
					UE_LOG(LogIris, VeryVerbose, TEXT("Calling RepNotify. Object: %s, Function: %s"), *Object->GetFullName(), ToCStr(RepNotifyFunction->GetName()));
					Object->ProcessEvent(const_cast<UFunction*>(RepNotifyFunction), const_cast<uint8*>(PrevValuePtr));
				}
			}
		}
	}
}

const TCHAR* FPropertyReplicationState::ToString(FStringBuilderBase& StringBuilder, bool bInIncludeAll) const
{
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;

		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const uint32 MemberCount = Descriptor->MemberCount;

		StringBuilder.Appendf(TEXT("FPropertyReplicationState %s\n"), Descriptor->DebugName->Name);

		const bool bIncludeAll = bInIncludeAll || Descriptor->IsInitState();
		FString TempString;
		for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
		{
			const FProperty* Property = MemberProperties[MemberIt];

			if (bIncludeAll || IsDirty(MemberIt))
			{
				Property->ExportTextItem_Direct(TempString, StateBuffer + Descriptor->MemberDescriptors[MemberIt].ExternalMemberOffset, nullptr, nullptr, PPF_SimpleObjectText);
				StringBuilder.Appendf(TEXT("%u - %s : %s\n"), MemberIt, *Property->GetName(), ToCStr(TempString));
				TempString.Reset();
			}
		}
	}

	return StringBuilder.ToString();
}

FString FPropertyReplicationState::ToString(bool bIncludeAll) const
{
	TStringBuilder<2048> Builder;
	ToString(Builder, bIncludeAll);

	return FString(Builder.ToString());
}

bool FPropertyReplicationState::IsCustomConditionEnabled(uint32 Index) const
{
	const EReplicationStateTraits Traits = ReplicationStateDescriptor->Traits;
	if (!EnumHasAnyFlags(Traits, EReplicationStateTraits::HasLifetimeConditionals))
	{
		return true;
	}

	const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
	const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskInfo = Descriptor->MemberChangeMaskDescriptors[Index];

	// $TODO: Test any bits if check triggers?
	checkSlow(ChangeMaskInfo.BitCount == 1);

	FNetBitArrayView MemberConditionalChangeMask = Private::GetMemberConditionalChangeMask(StateBuffer, Descriptor);
	return MemberConditionalChangeMask.GetBit(ChangeMaskInfo.BitOffset);
}

void FPropertyReplicationState::PollProperty(const void* SrcData, uint32 MemberIndex)
{
	if (IsValid())
	{
		const FReplicationStateDescriptor* Descriptor = ReplicationStateDescriptor;
		const uint8* SrcBuffer = static_cast<const uint8*>(SrcData);

		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const FProperty** MemberProperties = Descriptor->MemberProperties;
		const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
		const uint32 MemberCount = Descriptor->MemberCount;

		if (MemberIndex < MemberCount)
		{
			const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIndex];
			const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIndex];
			const FProperty* Property = MemberProperties[MemberIndex];

			//$TODO: make special version to avoid unnecessary overhead.
			SetPropertyValue(MemberIndex, SrcBuffer + Property->GetOffset_ForGC() + Property->ElementSize*MemberPropertyDescriptor.ArrayIndex);
		}
	}
}

}
