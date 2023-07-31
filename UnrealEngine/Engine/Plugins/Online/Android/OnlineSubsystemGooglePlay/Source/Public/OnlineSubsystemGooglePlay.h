// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineExternalUIInterfaceGooglePlay.h"
#include "OnlineAsyncTaskManagerGooglePlay.h"
#include "Templates/UniquePtr.h"
#include "HAL/RunnableThread.h"
#include "OnlineStoreGooglePlay.h"
#include "OnlinePurchaseGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/game_services.h"
#include "gpg/android_platform_configuration.h"
THIRD_PARTY_INCLUDES_END

#include <string>

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineIdentityGooglePlay,  ESPMode::ThreadSafe> FOnlineIdentityGooglePlayPtr;
typedef TSharedPtr<class FOnlineStoreGooglePlayV2, ESPMode::ThreadSafe> FOnlineStoreGooglePlayV2Ptr;
typedef TSharedPtr<class FOnlinePurchaseGooglePlay, ESPMode::ThreadSafe> FOnlinePurchaseGooglePlayPtr;
typedef TSharedPtr<class FOnlineLeaderboardsGooglePlay, ESPMode::ThreadSafe> FOnlineLeaderboardsGooglePlayPtr;
typedef TSharedPtr<class FOnlineAchievementsGooglePlay, ESPMode::ThreadSafe> FOnlineAchievementsGooglePlayPtr;
typedef TSharedPtr<class FOnlineExternalUIGooglePlay, ESPMode::ThreadSafe> FOnlineExternalUIGooglePlayPtr;

class FOnlineAsyncTaskGooglePlayLogin;
class FOnlineAsyncTaskGooglePlayShowLoginUI;
class FOnlineAsyncTaskGooglePlayLogout;
class FRunnableThread;

/**
 * OnlineSubsystemGooglePlay - Implementation of the online subsystem for Google Play services
 */
class ONLINESUBSYSTEMGOOGLEPLAY_API FOnlineSubsystemGooglePlay : 
	public FOnlineSubsystemImpl
{
public:

	virtual ~FOnlineSubsystemGooglePlay() = default;

	//~ Begin IOnlineSubsystem Interface
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const  override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override { return nullptr; }
	virtual IOnlineMessagePtr GetMessageInterface() const override { return nullptr; }
	virtual IOnlineSharingPtr GetSharingInterface() const override { return nullptr; }
	virtual IOnlineUserPtr GetUserInterface() const override { return nullptr; }
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override { return nullptr; }
	virtual IOnlineChatPtr GetChatInterface() const override { return nullptr; }
	virtual IOnlineStatsPtr GetStatsInterface() const override { return nullptr; }
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override { return nullptr; }
	virtual IOnlineTournamentPtr GetTournamentInterface() const override { return nullptr; }

	virtual class UObject* GetNamedInterface(FName InterfaceName) override { return nullptr; }
	virtual void SetNamedInterface(FName InterfaceName, class UObject* NewInterface) override {}
	virtual bool IsDedicated() const override { return false; }
	virtual bool IsServer() const override { return true; }
	virtual void SetForceDedicated(bool bForce) override {}
	virtual bool IsLocalPlayer(const FUniqueNetId& UniqueId) const override { return true; }

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual bool IsEnabled() const override;
	virtual FText GetOnlineServiceName() const override;
	//~ End IOnlineSubsystem Interface

	virtual bool Tick(float DeltaTime) override;
	
PACKAGE_SCOPE:

	FOnlineSubsystemGooglePlay() = delete;
	explicit FOnlineSubsystemGooglePlay(FName InInstanceName);

	/** Return the async task manager owned by this subsystem */
	class FOnlineAsyncTaskManagerGooglePlay* GetAsyncTaskManager() { return OnlineAsyncTaskThreadRunnable.Get(); }

	/**
	 * Add an async task onto the task queue for processing
	 * @param AsyncTask - new heap allocated task to process on the async task thread
	 */
	void QueueAsyncTask(class FOnlineAsyncTask* AsyncTask);

	/** Returns a pointer to the Google API entry point */
	gpg::GameServices* GetGameServices() const { return GameServicesPtr.get(); }

	/** Utility function, useful for Google APIs that take a std::string but we only have an FString */
	static std::string ConvertFStringToStdString(const FString& InString);

	/** Returns the Google Play-specific version of Identity, useful to avoid unnecessary casting */
	FOnlineIdentityGooglePlayPtr GetIdentityGooglePlay() const { return IdentityInterface; }

	/** Returns the Google Play-specific version of Achievements, useful to avoid unnecessary casting */
	FOnlineAchievementsGooglePlayPtr GetAchievementsGooglePlay() const { return AchievementsInterface; }

	/**
	 * Is IAP available for use
	 * @return true if IAP should be available, false otherwise
	 */
	bool IsInAppPurchasingEnabled();

	/** Returns true if there are any async login tasks currently being tracked. */
	bool AreAnyAsyncLoginTasksRunning() const { return CurrentLoginTask != nullptr || CurrentShowLoginUITask != nullptr; }

	/** Start a ShowLoginUI async task. Creates the GameServices object first if necessary. */
	void StartShowLoginUITask(int PlayerId, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate());

	/** Start a logout task if one isn't already in progress. */
	void StartLogoutTask(int32 LocalUserNum);

	/** Callback from JNI when Google Client is connected */
	void ProcessGoogleClientConnectResult(bool bInSuccessful, FString AccessToken);

private:

	/** Google callback when auth is complete */
	void OnAuthActionFinished(gpg::AuthOperation Op, gpg::AuthStatus Status);

	/** Android callback when an activity is finished */
	void OnActivityResult(JNIEnv *env, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data);

	/** Start a ShowLoginUI async task. */
	void StartShowLoginUITask_Internal(int PlayerId, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate());

	/** Online async task runnable */
	TUniquePtr<class FOnlineAsyncTaskManagerGooglePlay> OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	TUniquePtr<FRunnableThread> OnlineAsyncTaskThread;

	/** Interface to the online identity system */
	FOnlineIdentityGooglePlayPtr IdentityInterface;

	/** Interface to the online catalog */
	FOnlineStoreGooglePlayV2Ptr StoreV2Interface;

	/** Interface to the store purchasing */
	FOnlinePurchaseGooglePlayPtr PurchaseInterface;

	/** Interface to the online leaderboards */
	FOnlineLeaderboardsGooglePlayPtr LeaderboardsInterface;

	/** Interface to the online achievements */
	FOnlineAchievementsGooglePlayPtr AchievementsInterface;

	/** Interface to the external UI services */
	FOnlineExternalUIGooglePlayPtr ExternalUIInterface;

	/** Pointer to the main entry point for the Google API */
	std::unique_ptr<gpg::GameServices> GameServicesPtr;

	// Since the OnAuthActionFinished callback is "global" (within the scope of one GameServices object),
	// we track the async tasks that depend on this callback to notify us that they're finished.
	// Only one of these pointers should be non-null at any given time.

	/**
	 * Track the current login task (if any) so callbacks can notify it.
	 * Still owned by the AsyncTaskManager, do not delete via this pointer!
	 **/
	FOnlineAsyncTaskGooglePlayLogin* CurrentLoginTask;

	/**
	 * Track the current ShowLoginUI task (if any)
	 * Still owned by the AsyncTaskManager, do not delete via this pointer!
	 **/
	FOnlineAsyncTaskGooglePlayShowLoginUI* CurrentShowLoginUITask;

	/**
	 * Track the current Logout task
	 * Still owned by the AsyncTaskManager, do not delete via this pointer!
	 */
	FOnlineAsyncTaskGooglePlayLogout* CurrentLogoutTask;

	/** Handle of registered delegate for onActivityResult */
	FDelegateHandle OnActivityResultDelegateHandle;

	/** This task needs to be able to set the GameServicesPtr and clear CurrentLoginTask*/
	friend class FOnlineAsyncTaskGooglePlayLogin;

	/** This task needs to be able to clear CurrentShowLoginUITask */
	friend class FOnlineAsyncTaskGooglePlayShowLoginUI;

	/** This task needs to be able to clear CurrentLogoutTask */
	friend class FOnlineAsyncTaskGooglePlayLogout;

	gpg::AndroidPlatformConfiguration PlatformConfiguration;
};

typedef TSharedPtr<FOnlineSubsystemGooglePlay, ESPMode::ThreadSafe> FOnlineSubsystemGooglePlayPtr;
