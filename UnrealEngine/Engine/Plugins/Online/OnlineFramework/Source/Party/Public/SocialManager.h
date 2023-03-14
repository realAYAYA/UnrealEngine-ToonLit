// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemTypes.h"
#include "Engine/EngineBaseTypes.h"

#include "Party/PartyTypes.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Interactions/SocialInteractionHandle.h"

#include "SocialManager.generated.h"

class ULocalPlayer;
class USocialUser;
class USocialParty;
class USocialToolkit;
class UGameViewportClient;
class UGameInstance;
class FOnlineSessionSearchResult;
class FPartyPlatformSessionManager;
class USocialDebugTools;

enum ETravelType;

#define ABORT_DURING_SHUTDOWN() if (IsEngineExitRequested() || bShutdownPending) { UE_LOG(LogParty, Log, TEXT("%s - Received callback during shutdown: IsEngineExitRequested=%s, bShutdownPending=%s."), ANSI_TO_TCHAR(__FUNCTION__), *LexToString(IsEngineExitRequested()), *LexToString(bShutdownPending)); return; }

/** Singleton manager at the top of the social framework */
UCLASS(Within = GameInstance, Config = Game)
class PARTY_API USocialManager : public UObject, public FExec
{
	GENERATED_BODY()

	friend class FPartyPlatformSessionManager;
	friend UPartyMember;
	friend USocialUser;

public:
	// FExec
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out) override;

	static bool IsSocialSubsystemEnabled(ESocialSubsystem SubsystemType);
	static FName GetSocialOssName(ESocialSubsystem SubsystemType);
	static FText GetSocialOssPlatformName(ESocialSubsystem SubsystemType);
	static IOnlineSubsystem* GetSocialOss(UWorld* World, ESocialSubsystem SubsystemType);
	static FUserPlatform GetLocalUserPlatform();
	static const TArray<ESocialSubsystem>& GetDefaultSubsystems() { return DefaultSubsystems; }
	static const TArray<FSocialInteractionHandle>& GetRegisteredInteractions() { return RegisteredInteractions; }

	USocialManager();
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Initializes the manager - call this right after creating the manager object during GameInstance initialization. */
	virtual void InitSocialManager();
	virtual void ShutdownSocialManager();

	USocialToolkit& GetSocialToolkit(const ULocalPlayer& LocalPlayer) const;
	USocialToolkit* GetFirstLocalUserToolkit() const;
	FUniqueNetIdRepl GetFirstLocalUserId(ESocialSubsystem SubsystemType) const;
	bool IsLocalUser(const FUniqueNetIdRepl& LocalUserId, ESocialSubsystem SubsystemType) const;
	int32 GetFirstLocalUserNum() const;
	USocialDebugTools* GetDebugTools() const;

	DECLARE_EVENT_OneParam(USocialManager, FOnSocialToolkitCreated, USocialToolkit&)
	FOnSocialToolkitCreated& OnSocialToolkitCreated() const { return OnSocialToolkitCreatedEvent; }
	
	DECLARE_EVENT_OneParam(USocialManager, FOnPartyMembershipChanged, USocialParty&);
	FOnPartyMembershipChanged& OnPartyJoined() const { return OnPartyJoinedEvent; }

	DECLARE_DELEGATE_OneParam(FOnCreatePartyAttemptComplete, ECreatePartyCompletionResult);
	void CreateParty(const FOnlinePartyTypeId& PartyTypeId, const FPartyConfiguration& PartyConfig, const FOnCreatePartyAttemptComplete& OnCreatePartyComplete);
	void CreatePersistentParty(const FOnCreatePartyAttemptComplete& OnCreatePartyComplete = FOnCreatePartyAttemptComplete());

	/** Attempt to restore our party state from the party system */
	DECLARE_DELEGATE_OneParam(FOnRestorePartyStateFromPartySystemComplete, bool /*bSucceeded*/)
	void RestorePartyStateFromPartySystem(const FOnRestorePartyStateFromPartySystemComplete& OnRestoreComplete);

	bool IsPartyJoinInProgress(const FOnlinePartyTypeId& TypeId) const;
	bool IsPersistentPartyJoinInProgress() const;

	template <typename PartyT = USocialParty>
	PartyT* GetPersistentParty() const
	{
		return Cast<PartyT>(GetPersistentPartyInternal());
	}

	template <typename PartyT = USocialParty>
	PartyT* GetParty(const FOnlinePartyTypeId& PartyTypeId) const
	{
		return Cast<PartyT>(GetPartyInternal(PartyTypeId));
	}

	template <typename PartyT = USocialParty>
	PartyT* GetParty(const FOnlinePartyId& PartyId) const
	{
		return Cast<PartyT>(GetPartyInternal(PartyId));
	}

	bool IsConnectedToPartyService() const;

	void HandlePartyDisconnected(USocialParty* LeavingParty);

	/**
	 * Makes an attempt for the target local player to join the primary local player's party
	 * @param LocalPlayerNum - ControllerId of the Secondary player that wants to join the party
	 * @param Delegate - Delegate run when the join process is finished
	 */
	void RegisterSecondaryPlayer(int32 LocalPlayerNum, const FOnJoinPartyComplete& Delegate = FOnJoinPartyComplete());

	virtual void NotifyPartyInitialized(USocialParty& Party);

	/** Validates that the target user has valid join info for us to use and that we can join any party of the given type */
	virtual FJoinPartyResult ValidateJoinTarget(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId) const;

protected:
	DECLARE_DELEGATE_OneParam(FOnJoinPartyAttemptComplete, const FJoinPartyResult&);
	void JoinParty(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId, const FOnJoinPartyAttemptComplete& OnJoinPartyComplete, const FName& JoinMethod);

	USocialToolkit* GetSocialToolkit(int32 LocalPlayerNum) const;
	USocialToolkit* GetSocialToolkit(FUniqueNetIdRepl LocalUserId) const;

protected:
	struct PARTY_API FRejoinableParty : public TSharedFromThis<FRejoinableParty>
	{
		FRejoinableParty(const USocialParty& SourceParty);

		TSharedRef<const FOnlinePartyId> PartyId;
		TArray<FUniqueNetIdRef> MemberIds;
		FName OriginalJoinMethod;
	};

	struct PARTY_API FJoinPartyAttempt
	{
		FJoinPartyAttempt(TSharedRef<const FRejoinableParty> InRejoinInfo);
		FJoinPartyAttempt(const USocialUser* InTargetUser, const FOnlinePartyTypeId& InPartyTypeId, const FName& InJoinMethod, const FOnJoinPartyAttemptComplete& InOnJoinComplete);

		UE_DEPRECATED(5.1, "This constructor is deprecated, use (USocialUser*, FOnlinePartyTypeId, FName, FOnJoinPartyAttemptComplete) instead.")
		FJoinPartyAttempt(const USocialUser* InTargetUser, const FOnlinePartyTypeId& InPartyTypeId, const FOnJoinPartyAttemptComplete& InOnJoinComplete);

		FString ToDebugString() const;

		TWeakObjectPtr<const USocialUser> TargetUser;
		FOnlinePartyTypeId PartyTypeId;
		FName JoinMethod = PartyJoinMethod::Unspecified;
		FUniqueNetIdRepl TargetUserPlatformId;
		FSessionId PlatformSessionId;

		TSharedPtr<const FRejoinableParty> RejoinInfo;
		TSharedPtr<const IOnlinePartyJoinInfo> JoinInfo;

		FOnJoinPartyAttemptComplete OnJoinComplete;

		static const FName Step_FindPlatformSession;
		static const FName Step_QueryJoinability;
		static const FName Step_LeaveCurrentParty;
		static const FName Step_JoinParty;
		static const FName Step_DeferredPartyCreation;
		static const FName Step_WaitForPersistentPartyCreation;

		FSocialActionTimeTracker ActionTimeTracker;
	};

	virtual void RegisterSocialInteractions();

	/** Validate that we are clear to try joining a party of the given type. If not, gives the reason why. */
	virtual FJoinPartyResult ValidateJoinAttempt(const FOnlinePartyTypeId& PartyTypeId) const;
	
	/**
	 * Gives child classes a chance to append any additional data to a join request that's about to be sent to another party.
	 * This is where you'll add game-specific information that can affect whether you are eligible for the target party.
	 */
	virtual void FillOutJoinRequestData(const FOnlinePartyId& TargetParty, FOnlinePartyData& OutJoinRequestData) const;

	virtual TSubclassOf<USocialParty> GetPartyClassForType(const FOnlinePartyTypeId& PartyTypeId) const;

	//virtual void OnCreatePartyComplete(const TSharedPtr<const FOnlinePartyId>& PartyId, ECreatePartyCompletionResult Result, FOnlinePartyTypeId PartyTypeId) {}
	//virtual void OnQueryJoinabilityComplete(const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, FOnlinePartyTypeId PartyTypeId) {}

	virtual void OnJoinPartyAttemptCompleteInternal(const FJoinPartyAttempt& JoinAttemptInfo, const FJoinPartyResult& Result);
	virtual void OnPartyLeftInternal(USocialParty& LeftParty, EMemberExitedReason Reason) {}
	virtual void OnToolkitCreatedInternal(USocialToolkit& NewToolkit);

	virtual bool CanCreateNewPartyObjects() const;

	/** Up to the game to decide whether it wants to allow crossplay (generally based on a user setting of some kind) */
	virtual ECrossplayPreference GetCrossplayPreference() const;

	virtual bool ShouldTryRejoiningPersistentParty(const FRejoinableParty& InRejoinableParty) const;

	template <typename InteractionT>
	void RegisterInteraction()
	{
		RegisteredInteractions.Add(InteractionT::GetHandle());
	}

	void RefreshCanCreatePartyObjects();

	USocialParty* GetPersistentPartyInternal(bool bEvenIfLeaving = false) const;

public:
	const FJoinPartyAttempt* GetJoinAttemptInProgress(const FOnlinePartyTypeId& PartyTypeId) const;

protected:
	//@todo DanH: TEMP - for now relying on FN to bind to its game-level UFortOnlineSessionClient instance #required
	void HandlePlatformSessionInviteAccepted(const FUniqueNetIdRef& LocalUserId, const FOnlineSessionSearchResult& InviteResult);

	virtual TSubclassOf<USocialDebugTools> GetSocialDebugToolsClass() const;

	/** Info on the persistent party we were in when losing connection to the party service and want to rejoin when it returns */
	TSharedPtr<FRejoinableParty> RejoinableParty;

	/** The desired type of SocialToolkit to create for each local player */
	TSubclassOf<USocialToolkit> ToolkitClass;

	// Set during shutdown, used to early-out of lingering OnlineSubsystem callbacks that are pending
	bool bShutdownPending = false;

private:
	UGameInstance& GetGameInstance() const;
	USocialToolkit& CreateSocialToolkit(ULocalPlayer& OwningLocalPlayer, int32 LocalPlayerIndex);

	void QueryPartyJoinabilityInternal(FJoinPartyAttempt& JoinAttempt);
	void JoinPartyInternal(FJoinPartyAttempt& JoinAttempt);
	void FinishJoinPartyAttempt(FJoinPartyAttempt& JoinAttemptToDestroy, const FJoinPartyResult& JoinResult);
	
	USocialParty* EstablishNewParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnlinePartyTypeId& PartyTypeId);

	USocialParty* GetPartyInternal(const FOnlinePartyTypeId& PartyTypeId, bool bIncludeLeavingParties = false) const;
	USocialParty* GetPartyInternal(const FOnlinePartyId& PartyId, bool bIncludeLeavingParties = false) const;

	TSharedPtr<const IOnlinePartyJoinInfo> GetJoinInfoFromSession(const FOnlineSessionSearchResult& PlatformSession);

	void OnCreatePersistentPartyCompleteInternal(ECreatePartyCompletionResult Result, FOnCreatePartyAttemptComplete OnCreatePartyComplete);
	bool bCreatingPersistentParty = false;

private:	// Handlers
	void HandleGameViewportInitialized();
	void HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);
	void HandleWorldEstablished(UWorld* World);
	void HandleLocalPlayerAdded(int32 LocalUserNum);
	void HandleLocalPlayerRemoved(int32 LocalUserNum);
	void HandleToolkitReset(int32 LocalUserNum);
	
	void OnRestorePartiesComplete(const FUniqueNetId& LocalUserId, const FOnlineError& Result, const FOnRestorePartyStateFromPartySystemComplete OnRestoreComplete);
	void HandleCreatePartyComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& PartyId, ECreatePartyCompletionResult Result, FOnlinePartyTypeId PartyTypeId, FOnCreatePartyAttemptComplete CompletionDelegate);
	void HandleJoinPartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 NotApprovedReasonCode, FOnlinePartyTypeId PartyTypeId);
	
	void HandlePersistentPartyStateChanged(EPartyState NewState, EPartyState PreviousState, USocialParty* PersistentParty);
	void HandleLeavePartyForJoinComplete(ELeavePartyCompletionResult LeaveResult, USocialParty* LeftParty);
	void HandlePartyLeaveBegin(EMemberExitedReason Reason, USocialParty* LeavingParty);
	void HandlePartyLeft(EMemberExitedReason Reason, USocialParty* LeftParty);

	void HandleLeavePartyForMissingJoinAttempt(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnlinePartyTypeId PartyTypeId);

	void HandleFillPartyJoinRequestData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, FOnlinePartyData& PartyData);
	void HandleFindSessionForJoinComplete(bool bWasSuccessful, const FOnlineSessionSearchResult& FoundSession, FOnlinePartyTypeId PartyTypeId);

protected: // overridable handlers
	virtual void HandleQueryJoinabilityComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FQueryPartyJoinabilityResult& Result, FOnlinePartyTypeId PartyTypeId);

private:
	static TArray<ESocialSubsystem> DefaultSubsystems;
	static TArray<FSocialInteractionHandle> RegisteredInteractions;
	static TMap<TWeakObjectPtr<UGameInstance>, TWeakObjectPtr<USocialManager>> AllManagersByGameInstance;

	UPROPERTY()
	TArray<TObjectPtr<USocialToolkit>> SocialToolkits;

	UPROPERTY()
	TObjectPtr<USocialDebugTools> SocialDebugTools;

	bool bIsConnectedToPartyService = false;
	
	/**
	 * False during brief windows where the game isn't in a state conducive to creating a new party object and after the manager is completely shut down (prior to being GC'd)
	 * Tracked to allow OSS level party activity to execute immediately, but hold off on establishing our local (and replicated) awareness of the party until this client is ready.
	 */
	bool bCanCreatePartyObjects = false;

	TSharedPtr<FPartyPlatformSessionManager> PartySessionManager;

	TMap<FOnlinePartyTypeId, USocialParty*> JoinedPartiesByTypeId;
	TMap<FOnlinePartyTypeId, USocialParty*> LeavingPartiesByTypeId;
	TMap<FOnlinePartyTypeId, FJoinPartyAttempt> JoinAttemptsByTypeId;

	FDelegateHandle OnFillJoinRequestInfoHandle;

	mutable FOnSocialToolkitCreated OnSocialToolkitCreatedEvent;
	mutable FOnPartyMembershipChanged OnPartyJoinedEvent;
};