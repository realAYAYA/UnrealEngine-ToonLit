// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemNullPackage.h"

class FThreadSafeCounter;

class FOnlineAchievementsNull;
class FOnlineIdentityNull;
class FOnlineLeaderboardsNull;
class FOnlineSessionNull;
class FOnlineVoiceImpl;

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionNull, ESPMode::ThreadSafe> FOnlineSessionNullPtr;
typedef TSharedPtr<class FOnlineProfileNull, ESPMode::ThreadSafe> FOnlineProfileNullPtr;
typedef TSharedPtr<class FOnlineFriendsNull, ESPMode::ThreadSafe> FOnlineFriendsNullPtr;
typedef TSharedPtr<class FOnlineUserCloudNull, ESPMode::ThreadSafe> FOnlineUserCloudNullPtr;
typedef TSharedPtr<class FOnlineLeaderboardsNull, ESPMode::ThreadSafe> FOnlineLeaderboardsNullPtr;
typedef TSharedPtr<class FOnlineExternalUINull, ESPMode::ThreadSafe> FOnlineExternalUINullPtr;
typedef TSharedPtr<class FOnlineIdentityNull, ESPMode::ThreadSafe> FOnlineIdentityNullPtr;
typedef TSharedPtr<class FOnlineAchievementsNull, ESPMode::ThreadSafe> FOnlineAchievementsNullPtr;
typedef TSharedPtr<class FOnlineStoreV2Null, ESPMode::ThreadSafe> FOnlineStoreV2NullPtr;
typedef TSharedPtr<class FOnlinePurchaseNull, ESPMode::ThreadSafe> FOnlinePurchaseNullPtr;
typedef TSharedPtr<class FMessageSanitizerNull, ESPMode::ThreadSafe> FMessageSanitizerNullPtr;
#if WITH_ENGINE
typedef TSharedPtr<class FOnlineVoiceImpl, ESPMode::ThreadSafe> FOnlineVoiceImplPtr;
#endif //WITH_ENGINE

/**
 *	OnlineSubsystemNull - Implementation of the online subsystem for Null services
 */
class ONLINESUBSYSTEMNULL_API FOnlineSubsystemNull : 
	public FOnlineSubsystemImpl
{

public:

	virtual ~FOnlineSubsystemNull() = default;

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
	virtual IMessageSanitizerPtr GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const override;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;

	// FTSTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FOnlineSubsystemNull

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemNull() = delete;
	explicit FOnlineSubsystemNull(FName InInstanceName) :
		FOnlineSubsystemImpl(NULL_SUBSYSTEM, InInstanceName),
		SessionInterface(nullptr),
		VoiceInterface(nullptr),
		bVoiceInterfaceInitialized(false),
		LeaderboardsInterface(nullptr),
		IdentityInterface(nullptr),
		AchievementsInterface(nullptr),
		StoreV2Interface(nullptr),
		MessageSanitizerInterface(nullptr),
		OnlineAsyncTaskThreadRunnable(nullptr),
		OnlineAsyncTaskThread(nullptr)
	{}

	// Options for emulating different types of online platforms, these are settable via OSSNull cvars or in the [OnlineSubsystemNull] config section

	/** True if it should login the first user at startup like single-user platforms, false to only login when requested */
	static bool bAutoLoginAtStartup;

	/** True if it should support an external UI interface */
	static bool bSupportExternalUI;

	/** True if login requires calling ShowLoginUI on the externalUI, depends on SupportExternalUI */
	static bool bRequireShowLoginUI;

	/** True if the user index should change during login UI to emulate a platform user change */
	static bool bForceShowLoginUIUserChange;

	/** True if login should require a user/pass to act like an external service, false to match most platforms and use the default */
	static bool bRequireLoginCredentials;

	/** True if login name should include the local user number, which allows different stable IDs per user num */
	static bool bAddUserNumToNullId;

	/** True if it should use a system-stable null Id for login, same as -StableNullID on command line */
	static bool bForceStableNullId;

	/** True if it should fail faked network queries and act like an offline system */
	static bool bForceOfflineMode;

private:

	/** Interface to the session services */
	FOnlineSessionNullPtr SessionInterface;

	/** Interface for voice communication */
	mutable IOnlineVoicePtr VoiceInterface;

	/** Interface for voice communication */
	mutable bool bVoiceInterfaceInitialized;

	/** Interface to the leaderboard services */
	FOnlineLeaderboardsNullPtr LeaderboardsInterface;

	/** Interface to the identity registration/auth services */
	FOnlineIdentityNullPtr IdentityInterface;

	/** Interface to the identity registration/auth services */
	FOnlineExternalUINullPtr ExternalUIInterface;

	/** Interface for achievements */
	FOnlineAchievementsNullPtr AchievementsInterface;

	/** Interface for store */
	FOnlineStoreV2NullPtr StoreV2Interface;

	/** Interface for purchases */
	FOnlinePurchaseNullPtr PurchaseInterface;

	/** Interface for message sanitizing */
	FMessageSanitizerNullPtr MessageSanitizerInterface;

	/** Online async task runnable */
	class FOnlineAsyncTaskManagerNull* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

	// task counter, used to generate unique thread names for each task
	static FThreadSafeCounter TaskCounter;
};

typedef TSharedPtr<FOnlineSubsystemNull, ESPMode::ThreadSafe> FOnlineSubsystemNullPtr;


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
