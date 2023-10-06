// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialDebugTools.generated.h"

class FOnlinePartyId;
class FUniqueNetId;
class IOnlinePartyJoinInfo;
class USocialManager;

class IOnlineSubsystem;
class FOnlineAccountCredentials;
class IOnlinePartyPendingJoinRequestInfo;
struct FPartyMemberJoinInProgressRequest;
typedef TSharedPtr<const class IOnlinePartyJoinInfo> IOnlinePartyJoinInfoConstPtr;
typedef TSharedPtr<class FOnlinePartyData> FOnlinePartyDataPtr;
typedef TSharedPtr<const class FOnlinePartyData> FOnlinePartyDataConstPtr;
using FUniqueNetIdPtr = TSharedPtr<const FUniqueNetId>;
enum class EPartyJoinDenialReason : uint8;

UCLASS(Within = SocialManager, Config = Game)
class PARTY_API USocialDebugTools : public UObject, public FExec
{
	GENERATED_BODY()

	static constexpr const int32 LocalUserNum = 0;

public:
	USocialManager& GetSocialManager() const;

	// FExec
#if UE_ALLOW_EXEC_COMMANDS
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out) override;
#endif

	// USocialDebugTools

	USocialDebugTools();
	virtual void Shutdown();

	DECLARE_DELEGATE_OneParam(FLoginComplete, bool);
	virtual void Login(const FString& Instance, const FOnlineAccountCredentials& Credentials, const FLoginComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FLogoutComplete, bool);
	virtual void Logout(const FString& Instance, const FLogoutComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FJoinPartyComplete, bool);
	virtual void JoinParty(const FString& Instance, const FString& FriendName, const FJoinPartyComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FJoinInProgressComplete, EPartyJoinDenialReason);
	virtual void JoinInProgress(const FString& Instance, const FJoinInProgressComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FLeavePartyComplete, bool);
	virtual void LeaveParty(const FString& Instance, const FLeavePartyComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FCleanupPartiesComplete, bool);
	virtual void CleanupParties(const FString& Instance, const FCleanupPartiesComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FSetPartyMemberDataComplete, bool);
	virtual void SetPartyMemberData(const FString& Instance, const UStruct* StructType, const void* StructData, const FSetPartyMemberDataComplete& OnComplete);
	virtual void SetPartyMemberDataJson(const FString& Instance, const FString& JsonStr, const FSetPartyMemberDataComplete& OnComplete);

	virtual void GetContextNames(TArray<FString>& OutContextNames) const { Contexts.GenerateKeyArray(OutContextNames); }

	struct FInstanceContext
	{
		FInstanceContext(const FString& InstanceName, USocialDebugTools& SocialDebugTools)
			: Name(InstanceName)
			, OnlineSub(nullptr)
			, Owner(SocialDebugTools)
		{}

		void Init();
		void Shutdown();
		IOnlineSubsystem* GetOSS() const { return OnlineSub; }
		FOnlinePartyDataPtr GetPartyMemberData() const { return PartyMemberData; }
		FUniqueNetIdPtr GetLocalUserId() const;
		void ModifyPartyField(const FString& FieldName, const class FVariantData& FieldValue);

		bool SetJIPRequest(const FPartyMemberJoinInProgressRequest& InRequest);

		FString Name;
		IOnlineSubsystem* OnlineSub;
		USocialDebugTools& Owner;
		FOnlinePartyDataPtr PartyMemberData;

		// delegates
		FDelegateHandle LoginCompleteDelegateHandle;
		FDelegateHandle LogoutCompleteDelegateHandle;
		FDelegateHandle PresenceReceivedDelegateHandle;
		FDelegateHandle FriendInviteReceivedDelegateHandle;
		FDelegateHandle PartyInviteReceivedDelegateHandle;
		FDelegateHandle PartyJoinRequestReceivedDelegateHandle;
	};

	FInstanceContext& GetContext(const FString& Instance);
	FInstanceContext* GetContextForUser(const FUniqueNetId& UserId);

protected:
	virtual void PrintExecCommands() const;
	virtual bool RunCommand(const TCHAR* Cmd, const TArray<FString>& TargetInstances);
	virtual void NotifyContextInitialized(const FInstanceContext& Context) { }

private:

	bool bAutoAcceptFriendInvites;
	bool bAutoAcceptPartyInvites;

	TMap<FString, FInstanceContext> Contexts;

	IOnlinePartyJoinInfoConstPtr GetDefaultPartyJoinInfo() const;
	IOnlineSubsystem* GetDefaultOSS() const;
	void PrintExecUsage() const;

	// OSS callback handlers
	void HandleFriendInviteReceived(const FUniqueNetId& LocalUserId, const FUniqueNetId& FriendId);
	void HandlePartyInviteReceived(const FUniqueNetId& LocalUserId, const IOnlinePartyJoinInfo& Invitation);
	void HandlePartyJoinRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const IOnlinePartyPendingJoinRequestInfo& JoinRequestInfo);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "SocialTypes.h"
#endif
