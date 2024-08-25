// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectRuntime.h"

#include "SmartObjectComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectRuntime)

const FSmartObjectClaimHandle FSmartObjectClaimHandle::InvalidHandle = {};

namespace UE::SmartObject
{
TArray<FGameplayTag> GEnabledReasonTags;

uint16 GetMaskForEnabledReasonTag(const FGameplayTag Tag)
{
	int32 Index = GEnabledReasonTags.IndexOfByKey(Tag);
	if (Index == INDEX_NONE)
	{
		checkf(GEnabledReasonTags.Num() <= FSmartObjectRuntime::MaxNumDisableFlags,
			TEXT("Too many different Tags were used to control smart object enabled state. You need to limit to %d or change to a larger type for DisableFlags in FSmartObjectRuntime."),
			FSmartObjectRuntime::MaxNumDisableFlags);

		Index = GEnabledReasonTags.Add(Tag);
	}
	return (uint16)1 << Index;
}
}

//----------------------------------------------------------------------//
// FSmartObjectRuntime
//----------------------------------------------------------------------//
FSmartObjectRuntime::FSmartObjectRuntime(const USmartObjectDefinition& InDefinition)
	: Definition(&InDefinition)
{
}

bool FSmartObjectRuntime::IsEnabledForReason(const FGameplayTag ReasonTag) const
{
	if (ensureMsgf(ReasonTag.IsValid(), TEXT("%hs expects a valid tag. If the intent is to test the enabled state regardless of the reason use IsEnabled() instead."), __FUNCTION__))
	{
		const uint16 TagMask = UE::SmartObject::GetMaskForEnabledReasonTag(ReasonTag);
		return (DisableFlags & TagMask) == 0;
	}

	return DisableFlags == 0;
}

void FSmartObjectRuntime::SetEnabled(const bool bEnabled, const uint16 ReasonMask)
{
	if (bEnabled)
	{
		// Enabling so removing the flag
		DisableFlags &= ~ReasonMask;
	}
	else
	{
		// Disabling so adding the flag
		DisableFlags |= ReasonMask;
	}
}

void FSmartObjectRuntime::SetEnabled(const FGameplayTag ReasonTag, const bool bEnabled)
{
	if (ensureMsgf(ReasonTag.IsValid(), TEXT("%hs expects a valid tag."), __FUNCTION__))
	{
		const uint16 ReasonMask = UE::SmartObject::GetMaskForEnabledReasonTag(ReasonTag);
		SetEnabled(bEnabled, ReasonMask);
	}
}

bool FSmartObjectRuntime::ResolveOwnerActor() const
{
	if (OwnerComponent.IsExplicitlyNull())
	{
		if (const FSmartObjectActorOwnerData* ActorOwnerData = OwnerData.GetPtr<const FSmartObjectActorOwnerData>())
		{
			// Fetching the actor from the handle will either return the cached actor if already available
			// or will try to create one synchronously.
			// This actor is then expected to own a SmartObjectComponent that will register itself to the subsystem.
			// On successful registration the subsystem will update the OwnerComponent of the current runtime object.
			if (ActorOwnerData->Handle.FetchActor() != nullptr)
			{
				ensureMsgf(!OwnerComponent.IsExplicitlyNull(), TEXT("Successfully resolved actor is expected to register its smartobject component for '%s'."), *LexToString(GetRegisteredHandle()));
				return true;
			}
		}

		// Unable to find owner data or to fetch actor from the Actor instance handle
		return false;
	}

	// Success since it was already resolved
	return true;
}

AActor* FSmartObjectRuntime::GetOwnerActor(const ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated) const
{
	const USmartObjectComponent* Component = GetOwnerComponent(TrySpawnActorIfDehydrated);
	return Component != nullptr ? Component->GetOwner() : nullptr;
}

USmartObjectComponent* FSmartObjectRuntime::GetOwnerComponent(const ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated) const
{
	if (TrySpawnActorIfDehydrated == ETrySpawnActorIfDehydrated::Yes)
	{
		ResolveOwnerActor();
	}

	return OwnerComponent.Get();
}

#if WITH_SMARTOBJECT_DEBUG
FString FSmartObjectRuntime::DebugGetDisableFlagsString() const
{
	FStringBuilderBase DisableFlagsStringBuilder;
	for (int TagIndex = 0; TagIndex < MaxNumDisableFlags; ++TagIndex)
	{
		const uint16 TagMask = (uint16)1 << TagIndex;
		if (!!(DisableFlags & TagMask))
		{
			check(UE::SmartObject::GEnabledReasonTags.IsValidIndex(TagIndex));
			DisableFlagsStringBuilder += DisableFlagsStringBuilder.Len() ? TEXT(", ") : TEXT("Disabled by: ");
			DisableFlagsStringBuilder += UE::SmartObject::GEnabledReasonTags[TagIndex].ToString();
		}
	}
	return DisableFlagsStringBuilder.ToString();
}
#endif // WITH_SMARTOBJECT_DEBUG

//----------------------------------------------------------------------//
// FSmartObjectRuntimeSlot
//----------------------------------------------------------------------//
bool FSmartObjectRuntimeSlot::Claim(const FSmartObjectUserHandle& InUser, ESmartObjectClaimPriority ClaimPriority)
{
	if (CanBeClaimed(ClaimPriority))
	{
		State = ESmartObjectSlotState::Claimed;
		User = InUser;
		ClaimedPriority = ClaimPriority;
		return true;
	}
	return false;
}

bool FSmartObjectRuntimeSlot::Release(const FSmartObjectClaimHandle& ClaimHandle, const bool bAborted)
{
	if (!ensureMsgf(ClaimHandle.IsValid(), TEXT("Attempting to release a slot using an invalid handle: %s"), *LexToString(ClaimHandle)))
	{
		return false;
	}

	bool bReleased = false;

	if (State != ESmartObjectSlotState::Claimed && State != ESmartObjectSlotState::Occupied)
	{
		UE_LOG(LogSmartObject, Error, TEXT("Expected slot state is 'Claimed' or 'Occupied' but current state is '%s'. Slot will not be released"),
			*UEnum::GetValueAsString(State));
	}
	else if (ClaimHandle.UserHandle != User)
	{
		UE_LOG(LogSmartObject, Error, TEXT("User '%s' is trying to release slot claimed or used by other user '%s'. Slot will not be released"),
			*LexToString(ClaimHandle.UserHandle), *LexToString(User));
	}
	else
	{
		if (bAborted)
		{
			const bool bFunctionWasExecuted = OnSlotInvalidatedDelegate.ExecuteIfBound(ClaimHandle, State);
			UE_LOG(LogSmartObject, Verbose, TEXT("Slot invalidated callback was%scalled for %s"), bFunctionWasExecuted ? TEXT(" ") : TEXT(" not "), *LexToString(ClaimHandle));
		}

		State = ESmartObjectSlotState::Free;
		User.Invalidate();
		UserData.Reset();
		ClaimedPriority = ESmartObjectClaimPriority::None;
		bReleased = true;
	}

	return bReleased;
}
