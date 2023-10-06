// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectRuntime.h"

#include "SmartObjectComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectRuntime)

const FSmartObjectClaimHandle FSmartObjectClaimHandle::InvalidHandle = {};

//----------------------------------------------------------------------//
// FSmartObjectRuntime
//----------------------------------------------------------------------//
FSmartObjectRuntime::FSmartObjectRuntime(const USmartObjectDefinition& InDefinition)
	: Definition(&InDefinition)
	, bEnabled(true)
{
}

AActor* FSmartObjectRuntime::GetOwnerActor() const
{
	USmartObjectComponent* Component = OwnerComponent.Get();
	return Component != nullptr ? Component->GetOwner() : nullptr;
}


//----------------------------------------------------------------------//
// FSmartObjectRuntimeSlot
//----------------------------------------------------------------------//
bool FSmartObjectRuntimeSlot::Claim(const FSmartObjectUserHandle& InUser)
{
	if (CanBeClaimed())
	{
		State = ESmartObjectSlotState::Claimed;
		User = InUser;
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
		bReleased = true;
	}

	return bReleased;
}
