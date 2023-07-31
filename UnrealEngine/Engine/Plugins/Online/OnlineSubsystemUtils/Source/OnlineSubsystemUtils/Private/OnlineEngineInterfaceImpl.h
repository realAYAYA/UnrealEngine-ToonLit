// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Online/CoreOnline.h"
#include "Net/OnlineEngineInterface.h"
#include "OnlineEngineInterfaceImpl.generated.h"

class Error;
class FVoicePacket;
struct FWorldContext;

UCLASS(config = Engine)
class ONLINESUBSYSTEMUTILS_API UOnlineEngineInterfaceImpl : public UOnlineEngineInterface
{
	GENERATED_UCLASS_BODY()

	/**
	 * Subsystem
	 */
public:

	virtual bool IsLoaded(FName OnlineIdentifier) override;
	virtual FName GetOnlineIdentifier(FWorldContext& WorldContext) override;
	virtual bool DoesInstanceExist(FName OnlineIdentifier) override;
	virtual void ShutdownOnlineSubsystem(FName OnlineIdentifier) override;
	virtual void DestroyOnlineSubsystem(FName OnlineIdentifier) override;
	virtual FName GetDefaultOnlineSubsystemName() const override;
	virtual bool IsCompatibleUniqueNetId(const FUniqueNetIdWrapper& InUniqueNetId) const override;

	/**
	 * Utils
	 */
	virtual uint8 GetReplicationHashForSubsystem(FName InSubsystemName) const override;
	virtual FName GetSubsystemFromReplicationHash(uint8 InHash) const override;

private:
	/** Mapping of unique net ids that should not be treated as foreign ids to the local subsystem. */
	UPROPERTY(config)
	TMap<FName, FName> MappedUniqueNetIdTypes;

	/** Array of unique net ids that are deemed valid when tested against gameplay login checks. */
	UPROPERTY(config)
	TArray<FName> CompatibleUniqueNetIdTypes;
	
	/** Allow the subsystem used for voice functions to be overridden, in case it needs to be different than the default subsystem. May be useful on console platforms. */
	UPROPERTY(config)
	FName VoiceSubsystemNameOverride;

	/** @return the identifier/context handle associated with a UWorld */
	FName GetOnlineIdentifier(UWorld* World);

	/** Returns the name of a corresponding dedicated server subsystem for the given subsystem, or NAME_None if such a system doesn't exist. */
	FName GetDedicatedServerSubsystemNameForSubsystem(const FName Subsystem) const;

	/**
	 * Identity
	 */
public:

	virtual FUniqueNetIdWrapper CreateUniquePlayerIdWrapper(const FString& Str, FName Type) override;
	virtual FUniqueNetIdWrapper GetUniquePlayerIdWrapper(UWorld* World, int32 LocalUserNum, FName Type) override;

	virtual FString GetPlayerNickname(UWorld* World, const FUniqueNetIdWrapper& UniqueId) override;
	virtual bool GetPlayerPlatformNickname(UWorld* World, int32 LocalUserNum, FString& OutNickname) override;

	virtual bool AutoLogin(UWorld* World, int32 LocalUserNum, const FOnlineAutoLoginComplete& InCompletionDelegate) override;
	virtual bool IsLoggedIn(UWorld* World, int32 LocalUserNum) override;

private:

	FDelegateHandle OnLoginCompleteDelegateHandle;
	void OnAutoLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FName OnlineIdentifier, FOnlineAutoLoginComplete InCompletionDelegate);

	/**
	 * Session
	 */
public:

	virtual void StartSession(UWorld* World, FName SessionName, FOnlineSessionStartComplete& InCompletionDelegate) override;
	virtual void EndSession(UWorld* World, FName SessionName, FOnlineSessionEndComplete& InCompletionDelegate) override;
	virtual bool DoesSessionExist(UWorld* World, FName SessionName) override;

	virtual bool GetSessionJoinability(UWorld* World, FName SessionName, FJoinabilitySettings& OutSettings) override;
	virtual void UpdateSessionJoinability(UWorld* World, FName SessionName, bool bPublicSearchable, bool bAllowInvites, bool bJoinViaPresence, bool bJoinViaPresenceFriendsOnly) override;

	virtual void RegisterPlayer(UWorld* World, FName SessionName, const FUniqueNetIdWrapper& UniqueId, bool bWasInvited) override;
	virtual void UnregisterPlayer(UWorld* World, FName SessionName, const FUniqueNetIdWrapper& UniqueId) override;
	virtual void UnregisterPlayers(UWorld* World, FName SessionName, const TArray<FUniqueNetIdWrapper>& Players) override;

	virtual bool GetResolvedConnectString(UWorld* World, FName SessionName, FString& URL) override;

private:

	/** Mapping of delegate handles for each online StartSession() call while in flight */
	TMap<FName, FDelegateHandle> OnStartSessionCompleteDelegateHandles;
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful, FName OnlineIdentifier, FOnlineSessionStartComplete CompletionDelegate);

	/** Mapping of delegate handles for each online EndSession() call while in flight */
	TMap<FName, FDelegateHandle> OnEndSessionCompleteDelegateHandles;
	void OnEndSessionComplete(FName SessionName, bool bWasSuccessful, FName OnlineIdentifier, FOnlineSessionEndComplete CompletionDelegate);

	/**
	 * Voice
	 */
public:

	virtual TSharedPtr<FVoicePacket> GetLocalPacket(UWorld* World, uint8 LocalUserNum) override;
	virtual TSharedPtr<FVoicePacket> SerializeRemotePacket(UWorld* World, const UNetConnection* const RemoteConnection, FArchive& Ar) override;

	virtual void StartNetworkedVoice(UWorld* World, uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(UWorld* World, uint8 LocalUserNum) override;
	virtual void ClearVoicePackets(UWorld* World) override;

	virtual bool MuteRemoteTalker(UWorld* World, uint8 LocalUserNum, const FUniqueNetIdWrapper& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(UWorld* World, uint8 LocalUserNum, const FUniqueNetIdWrapper& PlayerId, bool bIsSystemWide) override;

	virtual int32 GetNumLocalTalkers(UWorld* World) override;

	/**
	 * External UI
	 */
public:

	virtual void ShowLeaderboardUI(UWorld* World, const FString& CategoryName) override;
	virtual void ShowAchievementsUI(UWorld* World, int32 LocalUserNum) override;
	virtual void BindToExternalUIOpening(const FOnlineExternalUIChanged& Delegate) override;
	virtual void ShowWebURL(const FString& CurrentURL, const UOnlineEngineInterface::FShowWebUrlParams& ShowParams, const FOnlineShowWebUrlClosed& CompletionDelegate) override;
	virtual bool CloseWebURL() override;

private:

	void OnExternalUIChange(bool bInIsOpening, FOnlineExternalUIChanged Delegate);

	/**
	 * Debug
	 */
public:

	virtual void DumpSessionState(UWorld* World) override;
	virtual void DumpPartyState(UWorld* World) override;
	virtual void DumpVoiceState(UWorld* World) override;
	virtual void DumpChatState(UWorld* World) override;

#if WITH_EDITOR
	/**
	 * PIE Utilities
	 */
public:

	virtual bool SupportsOnlinePIE() override;
	virtual void SetShouldTryOnlinePIE(bool bShouldTry) override;
	virtual int32 GetNumPIELogins() override;
	virtual void SetForceDedicated(FName OnlineIdentifier, bool bForce) override;
	virtual void LoginPIEInstance(FName OnlineIdentifier, int32 LocalUserNum, int32 PIELoginNum, FOnPIELoginComplete& CompletionDelegate) override;
#endif

private:

	/** Mapping of delegate handles for each online Login() call while in flight */
	TMap<FName, FDelegateHandle> OnLoginPIECompleteDelegateHandlesForPIEInstances;
	void OnPIELoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FName OnlineIdentifier, FOnlineAutoLoginComplete InCompletionDelegate);
};

