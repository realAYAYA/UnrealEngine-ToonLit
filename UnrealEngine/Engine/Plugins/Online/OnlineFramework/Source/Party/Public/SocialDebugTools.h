// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SocialTypes.h"
#include "SocialDebugTools.generated.h"

class IOnlineSubsystem;
class FOnlineAccountCredentials;
class IOnlinePartyPendingJoinRequestInfo;
typedef TSharedPtr<const class IOnlinePartyJoinInfo> IOnlinePartyJoinInfoConstPtr;
typedef TSharedPtr<class FOnlinePartyData> FOnlinePartyDataPtr;
typedef TSharedPtr<const class FOnlinePartyData> FOnlinePartyDataConstPtr;

UCLASS(Within = SocialManager, Config = Game)
class PARTY_API USocialDebugTools : public UObject, public FExec
{
	GENERATED_BODY()

	static const int32 LocalUserNum = 0;

public:
	USocialManager& GetSocialManager() const;

	// FExec
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out) override;

	// USocialDebugTools

	USocialDebugTools();
	virtual void Shutdown();

	DECLARE_DELEGATE_OneParam(FLoginComplete, bool);
	virtual void Login(const FString& Instance, const FOnlineAccountCredentials& Credentials, const FLoginComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FLogoutComplete, bool);
	virtual void Logout(const FString& Instance, const FLogoutComplete& OnComplete);

	DECLARE_DELEGATE_OneParam(FJoinPartyComplete, bool);
	virtual void JoinParty(const FString& Instance, const FString& FriendName, const FJoinPartyComplete& OnComplete);

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
		inline IOnlineSubsystem* GetOSS() { return OnlineSub; }
		inline FOnlinePartyDataConstPtr GetPartyMemberData() { return PartyMemberData; }

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
	virtual bool RunCommand(const TCHAR* Cmd, const TArray<FString>& TargetInstances);
	virtual void NotifyContextInitialized(const FInstanceContext& Context) { }

private:

	bool bAutoAcceptFriendInvites;
	bool bAutoAcceptPartyInvites;

	TMap<FString, FInstanceContext> Contexts;

	IOnlinePartyJoinInfoConstPtr GetDefaultPartyJoinInfo() const;
	IOnlineSubsystem* GetDefaultOSS() const;
	void PrintExecUsage() const;
	virtual void PrintExecCommands() const;

	// OSS callback handlers
	void HandleFriendInviteReceived(const FUniqueNetId& LocalUserId, const FUniqueNetId& FriendId);
	void HandlePartyInviteReceived(const FUniqueNetId& LocalUserId, const IOnlinePartyJoinInfo& Invitation);
	void HandlePartyJoinRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const IOnlinePartyPendingJoinRequestInfo& JoinRequestInfo);
};