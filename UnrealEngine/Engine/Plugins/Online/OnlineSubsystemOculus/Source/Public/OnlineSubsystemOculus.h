// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OnlineMessageTaskManagerOculus.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionOculus, ESPMode::ThreadSafe> FOnlineSessionOculusPtr;
typedef TSharedPtr<class FOnlineProfileOculus, ESPMode::ThreadSafe> FOnlineProfileOculusPtr;
typedef TSharedPtr<class FOnlineFriendsOculus, ESPMode::ThreadSafe> FOnlineFriendsOculusPtr;
typedef TSharedPtr<class FOnlineUserCloudOculus, ESPMode::ThreadSafe> FOnlineUserCloudOculusPtr;
typedef TSharedPtr<class FOnlineLeaderboardOculus, ESPMode::ThreadSafe> FOnlineLeaderboardsOculusPtr;
typedef TSharedPtr<class FOnlineVoiceOculus, ESPMode::ThreadSafe> FOnlineVoiceOculusPtr;
typedef TSharedPtr<class FOnlineExternalUIOculus, ESPMode::ThreadSafe> FOnlineExternalUIOculusPtr;
typedef TSharedPtr<class FOnlineIdentityOculus, ESPMode::ThreadSafe> FOnlineIdentityOculusPtr;
typedef TSharedPtr<class FOnlineAchievementsOculus, ESPMode::ThreadSafe> FOnlineAchievementsOculusPtr;

typedef TUniquePtr<class FOnlineMessageTaskManagerOculus> FOnlineMessageTaskManagerOculusPtr;

/**
*	OnlineSubsystemOculus - Implementation of the online subsystem for Oculus services
*/
class ONLINESUBSYSTEMOCULUS_API UE_DEPRECATED(5.2, "OnlineSubsystemOculus has been deprecated and will be removed") FOnlineSubsystemOculus :
	public FOnlineSubsystemImpl
{

public:

	virtual ~FOnlineSubsystemOculus() = default;

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineStatsPtr GetStatsInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual IOnlineTournamentPtr GetTournamentInterface() const override;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;

	// FTSTickerObjectBase

	virtual bool Tick(float DeltaTime) override;

	// FOnlineSubsystemOculus

	/**
	 * Allows for the LibOVRPlatform calls to be used directly with the Delegates in the Oculus OSS
	 */
	void AddRequestDelegate(ovrRequest RequestId, FOculusMessageOnCompleteDelegate&& Delegate) const;

	/**
	* Allows for direct subscription to the LibOVRPlatform notifications with the Delegates in the Oculus OSS
	*/
	FOculusMulticastMessageOnCompleteDelegate& GetNotifDelegate(ovrMessageType MessageType) const;
	void RemoveNotifDelegate(ovrMessageType MessageType, const FDelegateHandle& Delegate) const;

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemOculus() = delete;
	explicit FOnlineSubsystemOculus(FName InInstanceName) :
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FOnlineSubsystemImpl(OCULUS_SUBSYSTEM, InInstanceName),
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		bOculusInit(false)
	{}

	bool IsInitialized() const;

private:

	bool bOculusInit;

#if PLATFORM_WINDOWS
	bool InitWithWindowsPlatform() const;
#elif PLATFORM_ANDROID
	bool InitWithAndroidPlatform();
#endif

	/** Interface to the identity registration/auth services */
	FOnlineIdentityOculusPtr IdentityInterface;

	/** Interface to the session services */
	FOnlineSessionOculusPtr SessionInterface;

	/** Interface for achievements */
	FOnlineAchievementsOculusPtr AchievementsInterface;

	/** Interface for leaderboards */
	FOnlineLeaderboardsOculusPtr LeaderboardsInterface;

	/** Interface for friends */
	FOnlineFriendsOculusPtr FriendsInterface;

	/** Interface for CloudStorage User Saves. */
	FOnlineUserCloudOculusPtr UserCloudInterface;

	/** Interface for voice */
	FOnlineVoiceOculusPtr VoiceInterface;

	/** Message Task Manager */
	FOnlineMessageTaskManagerOculusPtr MessageTaskManager;
};

namespace FNetworkProtocolTypes
{
	ONLINESUBSYSTEMOCULUS_API extern const FLazyName Oculus;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
typedef TSharedPtr<FOnlineSubsystemOculus, ESPMode::ThreadSafe> FOnlineSubsystemOculusPtr;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
