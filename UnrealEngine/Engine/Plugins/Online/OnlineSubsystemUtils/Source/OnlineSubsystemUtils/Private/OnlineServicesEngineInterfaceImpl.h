// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Online/CoreOnline.h"
#include "Net/OnlineEngineInterface.h"
#include "OnlineServicesEngineInterfaceImpl.generated.h"

class Error;
class FVoicePacket;
struct FWorldContext;

/**
 * Implementation of UOnlineEngineInterface that uses Online Services (also known as Online Subsystem v2)
 */
UCLASS(config = Engine)
class ONLINESUBSYSTEMUTILS_API UOnlineServicesEngineInterfaceImpl : public UOnlineEngineInterface
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
	/** @return the identifier/context handle associated with a UWorld */
	FName GetOnlineIdentifier(UWorld* World) const;

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
};

