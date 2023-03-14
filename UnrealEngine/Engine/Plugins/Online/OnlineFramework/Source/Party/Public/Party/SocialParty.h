// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party/PartyTypes.h"
#include "Party/PartyDataReplicator.h"

#include "PartyBeaconState.h"
#include "SpectatorBeaconState.h"
#include "SpectatorBeaconClient.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Containers/Queue.h"
#include "Engine/EngineBaseTypes.h"
#include "SocialParty.generated.h"

class APartyBeaconClient;
class UNetDriver;

class ULocalPlayer;
class USocialManager;
class USocialUser;

class FOnlineSessionSettings;
class FOnlineSessionSearchResult;
enum class EMemberExitedReason : uint8;
struct FPartyMemberJoinInProgressRequest;
struct FPartyMemberJoinInProgressResponse;

/** Base struct used to replicate data about the state of the party to all members. */
USTRUCT()
struct PARTY_API FPartyRepData : public FOnlinePartyRepDataBase
{
	GENERATED_BODY();

public:
	FPartyRepData() {}
	void SetOwningParty(const class USocialParty& InOwnerParty);

	const FPartyPlatformSessionInfo* FindSessionInfo(const FString& SessionType) const;
	const TArray<FPartyPlatformSessionInfo>& GetPlatformSessions() const { return PlatformSessions; }
	FSimpleMulticastDelegate& OnPlatformSessionsChanged() const { return OnPlatformSessionsChangedEvent; } 

	void UpdatePlatformSessionInfo(FPartyPlatformSessionInfo&& SessionInfo);
	void ClearPlatformSessionInfo(const FString& SessionType);

protected:
	virtual bool CanEditData() const override;
	virtual void CompareAgainst(const FOnlinePartyRepDataBase& OldData) const override;
	virtual const USocialParty* GetOwnerParty() const override;

	TWeakObjectPtr<const USocialParty> OwnerParty;

	//@todo DanH Party: Isn't this redundant with the party config itself? Why bother putting it here too when the config replicates to everyone already? #suggested
	/** The privacy settings for the party */
	UPROPERTY()
	FPartyPrivacySettings PrivacySettings;
	EXPOSE_REP_DATA_PROPERTY(FPartyRepData, FPartyPrivacySettings, PrivacySettings);

	/** List of platform sessions for the party. Includes one entry per platform that needs a session and has a member of that session. */
	UPROPERTY()
	TArray<FPartyPlatformSessionInfo> PlatformSessions;

private:
	mutable FSimpleMulticastDelegate OnPlatformSessionsChangedEvent;
};

using FPartyDataReplicator = TPartyDataReplicator<FPartyRepData, USocialParty>;

/**
 * Party game state that contains all information relevant to the communication within a party
 * Keeps all players in sync with the state of the party and its individual members
 */
UCLASS(Abstract, Within=SocialManager, config=Game, Transient)
class PARTY_API USocialParty : public UObject
{
	GENERATED_BODY()

	friend class FPartyPlatformSessionMonitor;
	friend UPartyMember;
	friend USocialManager;
	friend USocialUser;
public:
	static bool IsJoiningDuringLoadEnabled();

	USocialParty();

	/** Re-evaluates whether this party is joinable by anyone and, if not, establishes the reason why */
	void RefreshPublicJoinability();

	DECLARE_DELEGATE_OneParam(FOnLeavePartyAttemptComplete, ELeavePartyCompletionResult)
	virtual void LeaveParty(const FOnLeavePartyAttemptComplete& OnLeaveAttemptComplete = FOnLeavePartyAttemptComplete());
	virtual void RemoveLocalMember(const FUniqueNetIdRepl& LocalUserId, const FOnLeavePartyAttemptComplete& OnLeaveAttemptComplete = FOnLeavePartyAttemptComplete());

	const FPartyRepData& GetRepData() const { return *PartyDataReplicator; }

	template <typename SocialManagerT = USocialManager>
	SocialManagerT& GetSocialManager() const
	{
		SocialManagerT* ManagerOuter = GetTypedOuter<SocialManagerT>();
		check(ManagerOuter);
		return *ManagerOuter;
	}
	
	template <typename MemberT = UPartyMember>
	MemberT& GetOwningLocalMember() const
	{
		MemberT* LocalMember = GetPartyMember<MemberT>(OwningLocalUserId);
		check(LocalMember);
		return *LocalMember;
	}

	template <typename MemberT = UPartyMember>
	MemberT* GetPartyLeader() const
	{
		return GetPartyMember<MemberT>(CurrentLeaderId);
	}

	template <typename MemberT = UPartyMember>
	MemberT* GetPartyMember(const FUniqueNetIdRepl& MemberId) const
	{
		return Cast<MemberT>(GetMemberInternal(MemberId));
	}

	bool ContainsUser(const USocialUser& User) const;

	ULocalPlayer& GetOwningLocalPlayer() const;
	const FUniqueNetIdRepl& GetOwningLocalUserId() const { return OwningLocalUserId; }
	const FUniqueNetIdRepl& GetPartyLeaderId() const { return CurrentLeaderId; }
	bool IsLocalPlayerPartyLeader() const;
	bool IsPartyLeader(const ULocalPlayer& LocalPlayer) const;
	bool IsPartyLeaderLocal() const;

	FChatRoomId GetChatRoomId() const;
	bool IsPersistentParty() const;
	const FOnlinePartyTypeId& GetPartyTypeId() const;
	const FOnlinePartyId& GetPartyId() const;

	EPartyState GetOssPartyState() const;
	EPartyState GetOssPartyPreviousState() const;

	bool IsCurrentlyCrossplaying() const;
	bool IsPartyFunctionalityDegraded() const;

	bool IsPartyFull() const;
	int32 GetNumPartyMembers() const;
	void SetPartyMaxSize(int32 NewSize);
	int32 GetPartyMaxSize() const;
	FPartyJoinDenialReason GetPublicJoinability() const;
	bool IsLeavingParty() const { return bIsLeavingParty; }

	/** Is the specified net driver for our reservation beacon? */
	bool IsNetDriverFromReservationBeacon(const UNetDriver* InNetDriver) const;

	virtual void DisconnectParty();

	template <typename MemberT = UPartyMember>
	TArray<MemberT*> GetPartyMembers() const
	{
		TArray<MemberT*> PartyMembers;
		PartyMembers.Reserve(PartyMembersById.Num());
		for (const auto& IdMemberPair : PartyMembersById)
		{
			if (MemberT* PartyMember = Cast<MemberT>(IdMemberPair.Value))
			{
				PartyMembers.Add(PartyMember);
			}
		}
		return PartyMembers;
	}

	FString ToDebugString() const;

	DECLARE_EVENT_OneParam(USocialParty, FLeavePartyEvent, EMemberExitedReason);
	FLeavePartyEvent& OnPartyLeaveBegin() const { return OnPartyLeaveBeginEvent; }
	FLeavePartyEvent& OnPartyLeft() const { return OnPartyLeftEvent; }

	DECLARE_EVENT(USocialParty, FDisconnectPartyEvent);
	FDisconnectPartyEvent& OnPartyDisconnected() const { return OnPartyDisconnectedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyMemberCreated, UPartyMember&);
	FOnPartyMemberCreated& OnPartyMemberCreated() const { return OnPartyMemberCreatedEvent; }

	DECLARE_EVENT_TwoParams(USocialParty, FOnPartyMemberLeft, UPartyMember*, const EMemberExitedReason);
	FOnPartyMemberLeft& OnPartyMemberLeft() const { return OnPartyMemberLeftEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyConfigurationChanged, const FPartyConfiguration&);
	FOnPartyConfigurationChanged& OnPartyConfigurationChanged() const { return OnPartyConfigurationChangedEvent; }

	DECLARE_EVENT_TwoParams(USocialParty, FOnPartyStateChanged, EPartyState, EPartyState);
	FOnPartyStateChanged& OnPartyStateChanged() const { return OnPartyStateChangedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyFunctionalityDegradedChanged, bool /*bFunctionalityDegraded*/);
	FOnPartyFunctionalityDegradedChanged& OnPartyFunctionalityDegradedChanged() const { return OnPartyFunctionalityDegradedChangedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnInviteSent, const USocialUser&);
	FOnInviteSent& OnInviteSent() const { return OnInviteSentEvent; }

	DECLARE_EVENT_TwoParams(USocialParty, FOnPartyJIPApproved, const FOnlinePartyId&, bool /* Success*/);
	UE_DEPRECATED(5.1, "Use the new join in progress flow with USocialParty::RequestJoinInProgress.")
	FOnPartyJIPApproved& OnPartyJIPApproved() const { return OnPartyJIPApprovedEvent; }

	DECLARE_EVENT_ThreeParams(USocialParty, FOnPartyJIPResponse, const FOnlinePartyId&, bool /* Success*/, const FString& /*DeniedResultCode*/);
	UE_DEPRECATED(5.1, "Use the new join in progress flow with USocialParty::RequestJoinInProgress.")
	FOnPartyJIPResponse& OnPartyJIPResponse() const { return OnPartyJIPResponseEvent; }

	DECLARE_EVENT_TwoParams(USocialParty, FOnPartyMemberConnectionStatusChanged, UPartyMember&, EMemberConnectionStatus);
	FOnPartyMemberConnectionStatusChanged& OnPartyMemberConnectionStatusChanged() const { return OnPartyMemberConnectionStatusChangedEvent; }

	void ResetPrivacySettings();
	const FPartyPrivacySettings& GetPrivacySettings() const;

	virtual bool ShouldAlwaysJoinPlatformSession(const FSessionId& SessionId) const;

	virtual void JoinSessionCompleteAnalytics(const FSessionId& SessionId, const FString& JoinBootableGroupSessionResult);
	bool IsCurrentlyLeaving() const;

	DECLARE_DELEGATE_OneParam(FOnRequestJoinInProgressComplete, const EPartyJoinDenialReason /*DenialReason*/);
	void RequestJoinInProgress(const UPartyMember& TargetMember, const FOnRequestJoinInProgressComplete& CompletionDelegate);

protected:
	void InitializeParty(const TSharedRef<const FOnlineParty>& InOssParty);
	bool IsInitialized() const;
	void TryFinishInitialization();

	bool ShouldCacheForRejoinOnDisconnect() const;

	void SetIsMissingPlatformSession(bool bInIsMissingPlatformSession);
	bool IsMissingPlatformSession() { return bIsMissingPlatformSession; }

	FPartyRepData& GetMutableRepData() { return *PartyDataReplicator; }

	//--------------------------
	// User/member-specific actions that are best exposed on the individuals themselves, but best handled by the actual party
	bool HasUserBeenInvited(const USocialUser& User) const;
	
	bool CanInviteUser(const USocialUser& User) const;
	bool CanPromoteMember(const UPartyMember& PartyMember) const;
	bool CanKickMember(const UPartyMember& PartyMember) const;
	
	bool TryInviteUser(const USocialUser& UserToInvite, const ESocialPartyInviteMethod InviteMethod = ESocialPartyInviteMethod::Other, const FString& MetaData = FString());
	bool TryPromoteMember(const UPartyMember& PartyMember);
	bool TryKickMember(const UPartyMember& PartyMember);
	//--------------------------

protected:
	virtual void InitializePartyInternal();

	FPartyConfiguration& GetCurrentConfiguration() { return CurrentConfig; }

	/** Only called when a new party is being created by the local player and they are responsible for the rep data. Otherwise we just wait to receive it from the leader. */
	virtual void InitializePartyRepData();
	virtual FPartyPrivacySettings GetDesiredPrivacySettings() const;
	static FPartyPrivacySettings GetPrivacySettingsForConfig(const FPartyConfiguration& PartyConfig);
	virtual void OnLocalPlayerIsLeaderChanged(bool bIsLeader);
	virtual void HandlePrivacySettingsChanged(const FPartyPrivacySettings& NewPrivacySettings);
	virtual void OnMemberCreatedInternal(UPartyMember& NewMember);
	virtual void OnLeftPartyInternal(EMemberExitedReason Reason);

	/** Virtual versions of the package-scoped "CanX" methods above, as a virtual declared within package scoping cannot link (exported public, imported protected) */
	virtual ESocialPartyInviteFailureReason CanInviteUserInternal(const USocialUser& User) const;
	virtual bool CanPromoteMemberInternal(const UPartyMember& PartyMember) const;
	virtual bool CanKickMemberInternal(const UPartyMember& PartyMember) const;

	virtual void OnInviteSentInternal(ESocialSubsystem SubsystemType, const USocialUser& InvitedUser, bool bWasSuccessful, const ESocialPartyInviteFailureReason FailureReason, const ESocialPartyInviteMethod InviteMethod);

	/* Deprecated version */
	virtual void OnInviteSentInternal(ESocialSubsystem SubsystemType, const USocialUser& InvitedUser, bool bWasSuccessful) {};

	virtual void HandlePartySystemStateChange(EPartySystemState NewState);

	/** Determines the joinability of this party for a group of users requesting to join */
	virtual FPartyJoinApproval EvaluateJoinRequest(const TArray<IOnlinePartyUserPendingJoinRequestInfoConstRef>& Players, bool bFromJoinRequest) const;

	/** Determines the joinability of the game a party is in for JoinInProgress */
	UE_DEPRECATED(5.1, "Use the new join in progress flow with USocialParty::RequestJoinInProgress.")
	virtual FPartyJoinApproval EvaluateJIPRequest(const FUniqueNetId& PlayerId) const;

	/** Determines the reason why, if at all, this party is currently flat-out unjoinable  */
	virtual FPartyJoinDenialReason DetermineCurrentJoinability() const;

	/** Override in child classes to specify the type of UPartyMember to create */
	virtual TSubclassOf<UPartyMember> GetDesiredMemberClass(bool bLocalPlayer) const;

	/** Override in child classes to provide encryption data for party beacon connections. */
	virtual bool InitializeBeaconEncryptionData(AOnlineBeaconClient& BeaconClient, const FString& SessionId);

	bool IsInviteRateLimited(const USocialUser& User, ESocialSubsystem SubsystemType) const;

	bool ApplyCrossplayRestriction(FPartyJoinApproval& JoinApproval, const FUserPlatform& Platform, const FOnlinePartyData& JoinData) const;
	FName GetGameSessionName() const;
	bool IsInRestrictedGameSession() const;

	/**
	 * Create a reservation beacon and connect to the server to get approval for new party members
	 * Only relevant while in an active game, not required while pre lobby / game
	 */
	void ConnectToReservationBeacon();
	void CleanupReservationBeacon();
	APartyBeaconClient* CreateReservationBeaconClient();

	APartyBeaconClient* GetReservationBeaconClient() const { return ReservationBeaconClient.Get(); }

	/**
	* Create a spectator beacon and connect to the server to get approval for new spectators
	*/
	void ConnectToSpectatorBeacon();
	void CleanupSpectatorBeacon();
	ASpectatorBeaconClient* CreateSpectatorBeaconClient();

	ASpectatorBeaconClient* GetSpectatorBeaconClient() const { return SpectatorBeaconClient.Get(); }

	/** Child classes MUST call EstablishRepDataInstance() on this using their member rep data struct instance */
	FPartyDataReplicator PartyDataReplicator;

	/** Reservation beacon class for getting server approval for new party members while in a game */
	UPROPERTY()
	TSubclassOf<APartyBeaconClient> ReservationBeaconClientClass;

	/** Spectator beacon class for getting server approval for new spectators while in a game */
	UPROPERTY()
		TSubclassOf<ASpectatorBeaconClient> SpectatorBeaconClientClass;

	/** Apply local party configuration to the OSS party, optionally resetting the access key to the party in the process */
	void UpdatePartyConfig(bool bResetAccessKey = false);

private:
	UPartyMember* GetOrCreatePartyMember(const FUniqueNetId& MemberId);
	void PumpApprovalQueue();
	void RejectAllPendingJoinRequests();
	void SetIsMissingXmppConnection(bool bInMissingXmppConnection);
	void BeginLeavingParty(EMemberExitedReason Reason);
	void FinalizePartyLeave(EMemberExitedReason Reason);

	void SetIsRequestingShutdown(bool bInRequestingShutdown);

	void CreatePlatformSession(const FString& SessionType);
	void UpdatePlatformSessionLeader(const FString& SessionType);

	void HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);

	UPartyMember* GetMemberInternal(const FUniqueNetIdRepl& MemberId) const;

private:	// Handlers
	void HandlePartyStateChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EPartyState PartyState, EPartyState PreviousPartyState);
	void HandlePartyConfigChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FPartyConfiguration& PartyConfig);
	void HandleUpdatePartyConfigComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EUpdateConfigCompletionResult Result);
	void HandlePartyDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FName& Namespace, const FOnlinePartyData& PartyData);
	void HandleJoinabilityQueryReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const IOnlinePartyPendingJoinRequestInfo& JoinRequestInfo);
	void HandlePartyJoinRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const IOnlinePartyPendingJoinRequestInfo& JoinRequestInfo);
	UE_DEPRECATED(5.1, "Use the new join in progress flow with USocialParty::RequestJoinInProgress.")
	void HandlePartyJIPRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId);
	void HandlePartyLeft(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId);
	void HandlePartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, EMemberExitedReason ExitReason);
	void HandlePartyMemberDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const FName& Namespace, const FOnlinePartyData& PartyMemberData);
	void HandlePartyMemberJoined(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId);
	UE_DEPRECATED(5.1, "Use the new join in progress flow with USocialParty::RequestJoinInProgress.")
	void HandlePartyMemberJIP(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool Success, int32 DeniedResultCode);
	void HandlePartyMemberPromoted(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& NewLeaderId);
	void HandlePartyPromotionLockoutChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool bArePromotionsLocked);

	void HandleMemberInitialized(UPartyMember* Member);
	void HandleMemberPlatformUniqueIdChanged(const FUniqueNetIdRepl& NewPlatformUniqueId, UPartyMember* Member);
	void HandleMemberSessionIdChanged(const FSessionId& NewSessionId, UPartyMember* Member);

	void HandleBeaconHostConnectionFailed();
	void HandleReservationRequestComplete(EPartyReservationResult::Type ReservationResponse);

	void HandleLeavePartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnLeavePartyAttemptComplete OnAttemptComplete);
	void HandleRemoveLocalPlayerComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnLeavePartyAttemptComplete OnAttemptComplete);

	void RemovePlayerFromReservationBeacon(const FUniqueNetId& LocalUserId, const FUniqueNetId& PlayerToRemove);

	void HandleJoinInProgressDataRequestChanged(const FPartyMemberJoinInProgressRequest& Request, UPartyMember* Member);
	void HandleJoinInProgressDataResponsesChanged(const TArray<FPartyMemberJoinInProgressResponse>& Responses, UPartyMember* Member);

private:
	TSharedPtr<const FOnlineParty> OssParty;

	UPROPERTY()
	FUniqueNetIdRepl OwningLocalUserId;

	/** Tracked explicitly so we know which player was demoted whenever the leader changes */
	UPROPERTY()
	FUniqueNetIdRepl CurrentLeaderId;

	UPROPERTY()
	TMap<FUniqueNetIdRepl, TObjectPtr<UPartyMember>> PartyMembersById;

	UPROPERTY(config)
	bool bEnableAutomaticPartyRejoin = true;

	TMap<FUniqueNetIdRepl, double> LastInviteSentById;

	UPROPERTY(config)
	double PlatformUserInviteCooldown = 10.f;

	UPROPERTY(config)
	double PrimaryUserInviteCooldown = 0.f;

	FPartyConfiguration CurrentConfig;

	//@todo DanH Party: Rename/reorg this to more clearly call out that this is specific to lobby beacon stuff #suggested
	struct FPendingMemberApproval
	{
		struct FMemberInfo
		{
			FMemberInfo(FUniqueNetIdRepl InMemberId, FUserPlatform InPlatform, TSharedPtr<const FOnlinePartyData> InJoinData = TSharedPtr<const FOnlinePartyData>())
				: MemberId(InMemberId)
				, Platform(MoveTemp(InPlatform))
				, JoinData(InJoinData)
			{}

			FUniqueNetIdRepl MemberId;
			FUserPlatform Platform;
			TSharedPtr<const FOnlinePartyData> JoinData;
		};

		FUniqueNetIdRepl RecipientId;
		TArray<FMemberInfo> Members;
		bool bIsJIPApproval;
		int64 JoinInProgressRequestTime = 0;
		bool bIsPlayerRemoval = false;
	};
	TQueue<FPendingMemberApproval> PendingApprovals;

	bool bStayWithPartyOnDisconnect = false;
	bool bIsMemberPromotionPossible = true;
	
	/**
	 * Last known reservation beacon client net driver name
	 * Intended to be used to detect network errors related to our current or last reservation beacon client's net driver.
	 * Some network error handlers may be called after we cleanup our beacon connection.
	 */
	FName LastReservationBeaconClientNetDriverName;
	
	/** Reservation beacon client instance while getting approval for new party members*/
	UPROPERTY()
	TWeakObjectPtr<APartyBeaconClient> ReservationBeaconClient = nullptr;
	
	/**
	* Last known spectator beacon client net driver name
	* Intended to be used to detect network errors related to our current or last spectator beacon client's net driver.
	* Some network error handlers may be called after we cleanup our beacon connection.
	*/
	FName LastSpectatorBeaconClientNetDriverName;
	
	/** Spectator beacon client instance while getting approval for spectator*/
	UPROPERTY()
	TWeakObjectPtr<ASpectatorBeaconClient> SpectatorBeaconClient = nullptr;

	/**
	 * True when we have limited functionality due to lacking an xmpp connection.
	 * Don't set directly, use the private setter to trigger events appropriately.
	 */
	TOptional<bool> bIsMissingXmppConnection;
	bool bIsMissingPlatformSession = false;

	bool bIsLeavingParty = false;
	bool bIsInitialized = false;
	bool bHasReceivedRepData = false;
	TOptional<bool> bIsRequestingShutdown;

	void RespondToJoinInProgressRequest(const FPendingMemberApproval& PendingApproval, const EPartyJoinDenialReason DenialReason);
	void CallJoinInProgressComplete(const EPartyJoinDenialReason DenialReason);
	void RunJoinInProgressTimer();

	/** Complete delegate for join in progress requests. This should only have one at a time. */
	TOptional<FOnRequestJoinInProgressComplete> RequestJoinInProgressComplete;

	FTimerHandle JoinInProgressTimerHandle;

	/** How often the timer should check in seconds for stale data when running. */
	UPROPERTY(config)
	float JoinInProgressTimerRate = 5.f;
	
	/** How long in seconds before join in progress requests timeout and are cleared from member data. */
	UPROPERTY(config)
	int32 JoinInProgressRequestTimeout = 30;

	/** How long in seconds before join in progress responses are cleared from member data. */
	UPROPERTY(config)
	int32 JoinInProgressResponseTimeout = 60;

	mutable FLeavePartyEvent OnPartyLeaveBeginEvent;
	mutable FLeavePartyEvent OnPartyLeftEvent;
	mutable FDisconnectPartyEvent OnPartyDisconnectedEvent;
	mutable FOnPartyMemberCreated OnPartyMemberCreatedEvent;
	mutable FOnPartyMemberLeft OnPartyMemberLeftEvent;
	mutable FOnPartyConfigurationChanged OnPartyConfigurationChangedEvent;
	mutable FOnPartyStateChanged OnPartyStateChangedEvent;
	mutable FOnPartyMemberConnectionStatusChanged OnPartyMemberConnectionStatusChangedEvent;
	mutable FOnPartyFunctionalityDegradedChanged OnPartyFunctionalityDegradedChangedEvent;
	mutable FOnInviteSent OnInviteSentEvent;
	mutable FOnPartyJIPApproved OnPartyJIPApprovedEvent;
	mutable FOnPartyJIPResponse OnPartyJIPResponseEvent;
};
