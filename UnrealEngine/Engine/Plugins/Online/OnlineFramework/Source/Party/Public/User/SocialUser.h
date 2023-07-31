// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interactions/SocialInteractionHandle.h"
#include "OnlineSessionSettings.h"
#include "Party/PartyTypes.h"
#include "SocialTypes.h"
#include "UObject/Object.h"

#include "SocialUser.generated.h"

class IOnlinePartyJoinInfo;
class FOnlineUserPresence;
class UPartyMember;
struct FOnlineError;
class IOnlinePartyRequestToJoinInfo;
enum class EPartyInvitationRemovedReason : uint8;
enum class EPlatformIconDisplayRule : uint8;
enum class ERequestToJoinPartyCompletionResult : int8;
enum class EPartyRequestToJoinRemovedReason : uint8;
typedef TSharedRef<const IOnlinePartyJoinInfo> IOnlinePartyJoinInfoConstRef;
typedef TSharedPtr<const IOnlinePartyJoinInfo> IOnlinePartyJoinInfoConstPtr;

namespace EOnlinePresenceState { enum Type : uint8; }

DECLARE_DELEGATE_OneParam(FOnNewSocialUserInitialized, USocialUser&);

UCLASS(Within = SocialToolkit)
class PARTY_API USocialUser : public UObject
{
	GENERATED_BODY()

	friend USocialToolkit;

public:
	USocialUser();

	void RegisterInitCompleteHandler(const FOnNewSocialUserInitialized& OnInitializationComplete);
	bool IsInitialized() const { return bIsInitialized; }

	void ValidateFriendInfo(ESocialSubsystem SubsystemType);
	TArray<ESocialSubsystem> GetRelationshipSubsystems(ESocialRelationship Relationship) const;
	TArray<ESocialSubsystem> GetRelevantSubsystems() const;
	bool HasSubsystemInfo(ESocialSubsystem Subsystem) const;
	bool HasSubsystemInfo(const TSet<ESocialSubsystem>& SubsystemTypes, bool bRequireAll = false);

	bool IsLocalUser() const;
	bool HasNetId(const FUniqueNetIdRepl& UniqueId) const;
	USocialToolkit& GetOwningToolkit() const;
	EOnlinePresenceState::Type GetOnlineStatus() const;

	FUniqueNetIdRepl GetUserId(ESocialSubsystem SubsystemType) const;
	FString GetDisplayName() const;
	FString GetDisplayName(ESocialSubsystem SubsystemType) const;
	virtual FString GetNickname() const;
	virtual bool SetNickname(const FString& InNickName);

	EInviteStatus::Type GetFriendInviteStatus(ESocialSubsystem SubsystemType) const;
	bool IsFriend() const;
	bool IsFriend(ESocialSubsystem SubsystemType) const;
	bool IsFriendshipPending(ESocialSubsystem SubsystemType) const;
	bool IsAnyInboundFriendshipPending() const;
	const FOnlineUserPresence* GetFriendPresenceInfo(ESocialSubsystem SubsystemType) const;
	FDateTime GetFriendshipCreationDate() const;
	virtual FDateTime GetLastOnlineDate() const;
	FText GetSocialName() const;
	virtual FUserPlatform GetCurrentPlatform() const;

	FString GetPlatformIconMarkupTag(EPlatformIconDisplayRule DisplayRule) const;
	virtual FString GetPlatformIconMarkupTag(EPlatformIconDisplayRule DisplayRule, FString& OutLegacyString) const;
	virtual FString GetMarkupTagForPlatform(const FUserPlatform& RemoteUserPlatform) const { return RemoteUserPlatform; }

	virtual void GetRichPresenceText(FText& OutRichPresence) const;

	bool IsRecentPlayer() const;
	bool IsRecentPlayer(ESocialSubsystem SubsystemType) const;
	
	bool IsBlocked() const;
	bool IsBlocked(ESocialSubsystem SubsystemType) const;

	bool IsOnline() const;
	bool IsPlayingThisGame() const;
	
	virtual bool CanReceiveOfflineInvite() const { return false; }
	virtual int64 GetInteractionScore() const { return 0;  }
	virtual int64 GetCustomSortValuePrimary() const { return 0; }
	virtual int64 GetCustomSortValueSecondary() const { return 0; }
	virtual int64 GetCustomSortValueTertiary() const { return 0; }

	/** Populate list with sort values in order of priority */
	virtual void PopulateSortParameterList(TArray<int64>& OutSortParams) const;

	bool SetUserLocalAttribute(ESocialSubsystem SubsystemType, const FString& AttrName, const FString& AttrValue);
	bool GetUserAttribute(ESocialSubsystem SubsystemType, const FString& AttrName, FString& OutAttrValue) const;

	bool HasAnyInteractionsAvailable() const;
	TArray<FSocialInteractionHandle> GetAllAvailableInteractions() const;

	virtual bool CanSendFriendInvite(ESocialSubsystem SubsystemType) const;
	virtual bool SendFriendInvite(ESocialSubsystem SubsystemType);
	virtual bool AcceptFriendInvite(ESocialSubsystem SocialSubsystem) const;
	virtual bool RejectFriendInvite(ESocialSubsystem SocialSubsystem) const;
	virtual bool EndFriendship(ESocialSubsystem SocialSubsystem) const;

	TMap<FString, FString> GetAnalyticsContext() const { return AnalyticsContext; }
	void WithContext(const TMap<FString, FString>& InAnalyticsContext, void(*Func)(USocialUser&));

	bool ShowPlatformProfile();

	void HandlePartyInviteReceived(const IOnlinePartyJoinInfo& Invite);
	void HandlePartyInviteRemoved(const IOnlinePartyJoinInfo& Invite, EPartyInvitationRemovedReason Reason);

	virtual bool CanRequestToJoin() const { return false; }
	virtual bool HasRequestedToJoinUs() const { return false; }
	void HandleRequestToJoinReceived(const IOnlinePartyRequestToJoinInfo& Request);
	void HandleRequestToJoinRemoved(const IOnlinePartyRequestToJoinInfo& Request, EPartyRequestToJoinRemovedReason Reason);

	UE_DEPRECATED(5.1, "Use RequestToJoinParty(const FName&) instead of RequestToJoinParty(void)")
	void RequestToJoinParty();

	virtual void RequestToJoinParty(const FName& JoinMethod);
	void AcceptRequestToJoinParty() const;
	void DismissRequestToJoinParty() const;
	virtual void HandlePartyRequestToJoinSent(const FUniqueNetId& LocalUserId, const FUniqueNetId& PartyLeaderId, const FDateTime& ExpiresAt, const ERequestToJoinPartyCompletionResult Result, FName JoinMethod);
	
	virtual IOnlinePartyJoinInfoConstPtr GetPartyJoinInfo(const FOnlinePartyTypeId& PartyTypeId) const;

	bool HasSentPartyInvite(const FOnlinePartyTypeId& PartyTypeId) const;
	FJoinPartyResult CheckPartyJoinability(const FOnlinePartyTypeId& PartyTypeId) const;

	UE_DEPRECATED(5.1, "Use JoinParty(const FOnlinePartyTypeId&, const FName&) instead of JoinParty(const FOnlinePartyTypeId&)")
	void JoinParty(const FOnlinePartyTypeId& PartyTypeId) const;

	virtual void JoinParty(const FOnlinePartyTypeId& PartyTypeId, const FName& JoinMethod) const;
	virtual void RejectPartyInvite(const FOnlinePartyTypeId& PartyTypeId);

	bool HasBeenInvitedToParty(const FOnlinePartyTypeId& PartyTypeId) const;
	bool CanInviteToParty(const FOnlinePartyTypeId& PartyTypeId) const;
	bool InviteToParty(const FOnlinePartyTypeId& PartyTypeId, const ESocialPartyInviteMethod InviteMethod = ESocialPartyInviteMethod::Other, const FString& MetaData = FString()) const;

	virtual bool BlockUser(ESocialSubsystem Subsystem) const;
	virtual bool UnblockUser(ESocialSubsystem Subsystem) const;

	UPartyMember* GetPartyMember(const FOnlinePartyTypeId& PartyTypeId) const;

	DECLARE_EVENT_OneParam(USocialUser, FOnNicknameChanged, const FText&);
	FOnNicknameChanged& OnSetNicknameCompleted() const { return OnSetNicknameCompletedEvent; }

	DECLARE_EVENT(USocialUser, FPartyInviteResponseEvent);
	FPartyInviteResponseEvent& OnPartyInviteAccepted() const { return OnPartyInviteAcceptedEvent; }
	FPartyInviteResponseEvent& OnPartyInviteRejected() const { return OnPartyInviteRejectedEvent; }

	DECLARE_EVENT_OneParam(USocialUser, FOnUserPresenceChanged, ESocialSubsystem)
	FOnUserPresenceChanged& OnUserPresenceChanged() const { return OnUserPresenceChangedEvent; }

	// provided so that lists with custom game-specific filtering (and any other listeners) can potentially re-evaluate a user
	// the pattern here is similar to OnUserPresenceChanged but not subsystem-specific
	DECLARE_EVENT(USocialUser, FOnUserGameSpecificStatusChanged)
	FOnUserGameSpecificStatusChanged& OnUserGameSpecificStatusChanged() const { return OnUserGameSpecificStatusChangedEvent; }

	DECLARE_EVENT_OneParam(USocialUser, FOnFriendRemoved, ESocialSubsystem)
	FOnFriendRemoved& OnFriendRemoved() const { return OnFriendRemovedEvent; }
	FOnFriendRemoved& OnFriendInviteRemoved() const { return OnFriendInviteRemovedEvent; }

	DECLARE_EVENT_TwoParams(USocialUser, FOnBlockedStatusChanged, ESocialSubsystem, bool)
	FOnBlockedStatusChanged& OnBlockedStatusChanged() const { return OnBlockedStatusChangedEvent; }

	DECLARE_EVENT_ThreeParams(USocialUser, FOnSubsystemIdEstablished, USocialUser&, ESocialSubsystem, const FUniqueNetIdRepl&)
	FOnSubsystemIdEstablished& OnSubsystemIdEstablished() const { return OnSubsystemIdEstablishedEvent; }

	//void ClearPopulateInfoDelegateForSubsystem(ESocialSubsystem SubsystemType);

	FString ToDebugString() const;

	void EstablishOssInfo(const TSharedRef<FOnlineFriend>& FriendInfo, ESocialSubsystem SubsystemType);
	void EstablishOssInfo(const TSharedRef<FOnlineBlockedPlayer>& BlockedPlayerInfo, ESocialSubsystem SubsystemType);
	void EstablishOssInfo(const TSharedRef<FOnlineRecentPlayer>& RecentPlayerInfo, ESocialSubsystem SubsystemType);

protected:
	void InitLocalUser();
	void Initialize(const FUniqueNetIdRepl& PrimaryId);

	void NotifyPresenceChanged(ESocialSubsystem SubsystemType);
	void NotifyUserUnblocked(ESocialSubsystem SubsystemType);
	void NotifyFriendInviteRemoved(ESocialSubsystem SubsystemType);
	void NotifyUserUnfriended(ESocialSubsystem SubsystemType);

#if WITH_EDITOR
	void Debug_RandomizePresence();
	bool bDebug_IsPresenceArtificial = false;
	EOnlinePresenceState::Type Debug_RandomPresence;
#endif

protected:
	virtual void OnPresenceChangedInternal(ESocialSubsystem SubsystemType);
	virtual void OnPartyInviteAcceptedInternal(const FOnlinePartyTypeId& PartyTypeId) const;
	virtual void OnPartyInviteRejectedInternal(const FOnlinePartyTypeId& PartyTypeId) const;
	virtual void HandleSetNicknameComplete(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnlineError& Error);
	virtual void SetSubsystemId(ESocialSubsystem SubsystemType, const FUniqueNetIdRepl& SubsystemId);

	UE_DEPRECATED(5.1, "This function is deprecated and will be removed.")
	virtual void NotifyRequestToJoinSent(const FDateTime& ExpiresAt) {}

	virtual void NotifyRequestToJoinReceived(const IOnlinePartyRequestToJoinInfo& Request) {}
	virtual void NotifyRequestToJoinRemoved(const IOnlinePartyRequestToJoinInfo& Request, EPartyRequestToJoinRemovedReason Reason) {}
	int32 NumPendingQueries = 0;
	TMap<FString, FString> AnalyticsContext;

	void TryBroadcastInitializationComplete();

	struct FSubsystemUserInfo
	{
		FSubsystemUserInfo(const FUniqueNetIdRepl& InUserId)
			: UserId(InUserId)
		{}

		bool IsValid() const;
		const FUniqueNetIdRepl& GetUserId() const { return UserId; }
		FString GetDisplayName() const { return UserInfo.IsValid() ? UserInfo.Pin()->GetDisplayName() : TEXT(""); }
		bool IsFriend() const { return GetFriendInviteStatus() == EInviteStatus::Accepted; }
		bool IsBlocked() const { return BlockedPlayerInfo.IsValid() || GetFriendInviteStatus() == EInviteStatus::Blocked; }
		EInviteStatus::Type GetFriendInviteStatus() const { return FriendInfo.IsValid() ? FriendInfo.Pin()->GetInviteStatus() : EInviteStatus::Unknown; }
		bool HasValidPresenceInfo() const { return IsFriend(); }
		const FOnlineUserPresence* GetPresenceInfo() const;

		// On the fence about caching this locally. We don't care about where it came from if we do, and we can cache it independent from any of the info structs (which will play nice with external mapping queries before querying the user info itself)
		FUniqueNetIdRepl UserId;

		TWeakPtr<FOnlineUser> UserInfo;
		TWeakPtr<FOnlineFriend> FriendInfo;
		TWeakPtr<FOnlineRecentPlayer> RecentPlayerInfo;
		TWeakPtr<FOnlineBlockedPlayer> BlockedPlayerInfo;
	};
	const FSubsystemUserInfo* GetSubsystemUserInfo(ESocialSubsystem Subsystem) const { return SubsystemInfoByType.Find(Subsystem); }

private:
	void SetUserInfo(ESocialSubsystem SubsystemType, const TSharedRef<FOnlineUser>& UserInfo);
	void HandleQueryUserInfoComplete(ESocialSubsystem SubsystemType, bool bWasSuccessful, const TSharedPtr<FOnlineUser>& UserInfo);

	virtual FString SanitizePresenceString(FString InString) const;

private:
	FSubsystemUserInfo& FindOrCreateSubsystemInfo(const FUniqueNetIdRepl& SubsystemId, ESocialSubsystem SubsystemType);

	bool bIsInitialized = false;

	TMap<ESocialSubsystem, FSubsystemUserInfo> SubsystemInfoByType;

	TArray<IOnlinePartyJoinInfoConstRef> ReceivedPartyInvites;

	// Initialization delegates that fire only when a specific user has finishing initializing
	static TMap<TWeakObjectPtr<USocialUser>, TArray<FOnNewSocialUserInitialized>> InitEventsByUser;

	mutable FOnNicknameChanged OnSetNicknameCompletedEvent;
	mutable FPartyInviteResponseEvent OnPartyInviteAcceptedEvent;
	mutable FPartyInviteResponseEvent OnPartyInviteRejectedEvent;
	mutable FOnUserPresenceChanged OnUserPresenceChangedEvent;
	mutable FOnFriendRemoved OnFriendRemovedEvent;
	mutable FOnFriendRemoved OnFriendInviteRemovedEvent;
	mutable FOnBlockedStatusChanged OnBlockedStatusChangedEvent;
	mutable FOnSubsystemIdEstablished OnSubsystemIdEstablishedEvent;
	mutable FOnUserGameSpecificStatusChanged OnUserGameSpecificStatusChangedEvent;
};
