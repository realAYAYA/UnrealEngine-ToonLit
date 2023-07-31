// Copyright Epic Games, Inc. All Rights Reserved.

#include "Party/PartyTypes.h"
#include "Party/SocialParty.h"
#include "Party/PartyMember.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PartyTypes)

//////////////////////////////////////////////////////////////////////////
// FPartyPlatformSessionInfo
//////////////////////////////////////////////////////////////////////////

bool FPartyPlatformSessionInfo::operator==(const FPartyPlatformSessionInfo& Other) const
{
	return SessionType == Other.SessionType
		&& SessionId == Other.SessionId
		&& OwnerPrimaryId == Other.OwnerPrimaryId;
}

bool FPartyPlatformSessionInfo::operator==(const FString& InSessionType) const
{
	return SessionType == InSessionType;
}

FString FPartyPlatformSessionInfo::ToDebugString() const
{
	return FString::Printf(TEXT("SessionType=[%s], SessionId=[%s], OwnerPrimaryId=[%s]"), *SessionType, *SessionId, *OwnerPrimaryId.ToDebugString());
}

bool FPartyPlatformSessionInfo::IsSessionOwner(const UPartyMember& PartyMember) const
{
	return PartyMember.GetPrimaryNetId() == OwnerPrimaryId;
}

bool FPartyPlatformSessionInfo::IsInSession(const UPartyMember& PartyMember) const
{
	return PartyMember.GetRepData().GetPlatformDataSessionId() == SessionId;
}

//////////////////////////////////////////////////////////////////////////
// FPartyPrivacySettings
//////////////////////////////////////////////////////////////////////////

bool FPartyPrivacySettings::operator==(const FPartyPrivacySettings& Other) const
{
	return PartyType == Other.PartyType
		&& PartyInviteRestriction == Other.PartyInviteRestriction
		&& bOnlyLeaderFriendsCanJoin == Other.bOnlyLeaderFriendsCanJoin;
}

//////////////////////////////////////////////////////////////////////////
// FJoinPartyResult
//////////////////////////////////////////////////////////////////////////

FJoinPartyResult::FJoinPartyResult()
	: Result(EJoinPartyCompletionResult::Succeeded)
{
}

FJoinPartyResult::FJoinPartyResult(FPartyJoinDenialReason InDenialReason)
{
	SetDenialReason(InDenialReason);
}

FJoinPartyResult::FJoinPartyResult(EJoinPartyCompletionResult InResult)
	: Result(InResult)
{
}

FJoinPartyResult::FJoinPartyResult(EJoinPartyCompletionResult InResult, FPartyJoinDenialReason InDenialReason)
{
	SetResult(InResult);
	if (InResult == EJoinPartyCompletionResult::NotApproved)
	{
		SetDenialReason(InDenialReason);
	}
}

FJoinPartyResult::FJoinPartyResult(EJoinPartyCompletionResult InResult, int32 InResultSubCode)
{
	SetResult(InResult);
	if (InResult == EJoinPartyCompletionResult::NotApproved)
	{
		SetDenialReason(InResultSubCode);
	}
	else
	{
		ResultSubCode = InResultSubCode;
	}
}

void FJoinPartyResult::SetDenialReason(FPartyJoinDenialReason InDenialReason)
{
	DenialReason = InDenialReason;
	if (InDenialReason.HasAnyReason())
	{
		Result = EJoinPartyCompletionResult::NotApproved;
	}
}

void FJoinPartyResult::SetResult(EJoinPartyCompletionResult InResult)
{
	Result = InResult;
	if (InResult != EJoinPartyCompletionResult::NotApproved)
	{
		DenialReason = EPartyJoinDenialReason::NoReason;
	}
}

bool FJoinPartyResult::WasSuccessful() const
{
	return Result == EJoinPartyCompletionResult::Succeeded;
}

//////////////////////////////////////////////////////////////////////////
// FOnlinePartyRepDataBase
//////////////////////////////////////////////////////////////////////////

void FOnlinePartyRepDataBase::LogSetPropertyFailure(const TCHAR* OwningStructTypeName, const TCHAR* PropertyName) const
{
	const USocialParty* OwningParty = GetOwnerParty();
	UE_LOG(LogParty, Warning, TEXT("Failed to modify RepData property [%s::%s] in party [%s] - local member [%s] does not have authority."),
		OwningStructTypeName,
		PropertyName,
		OwningParty ? *OwningParty->ToDebugString() : TEXT("unknown"),
		OwningParty ? *OwningParty->GetOwningLocalMember().ToDebugString(false) : TEXT("unknown"));
}

void FOnlinePartyRepDataBase::LogPropertyChanged(const TCHAR* OwningStructTypeName, const TCHAR* PropertyName, bool bFromReplication) const
{
	const USocialParty* OwningParty = GetOwnerParty();
	
	// Only thing this lacks is the id of the party member for member rep data changes
	UE_LOG(LogParty, VeryVerbose, TEXT("RepData property [%s::%s] changed %s in party [%s]"),
		OwningStructTypeName,
		PropertyName,
		bFromReplication ? TEXT("remotely") : TEXT("locally"),
		OwningParty ? *OwningParty->ToDebugString() : TEXT("unknown"));
}

