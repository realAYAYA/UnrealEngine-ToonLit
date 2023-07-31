// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlinePlayTimeLimit.h"
#include "PlayTimeLimitUser.h"
#include "Containers/Ticker.h"

/**
 * Configuration
 */
struct FOnlinePlayLimitConfigEntry
{
	/** Constructor */
	FOnlinePlayLimitConfigEntry(int32 InTimeStartMinutes, int32 InNotificationRateMinutes, float InRewardRate)
		: TimeStartMinutes(InTimeStartMinutes)
		, NotificationRateMinutes(InNotificationRateMinutes)
		, RewardRate(InRewardRate)
	{}

	/** Number of minutes the user must play before this is effective */
	int32 TimeStartMinutes;
	/** Number of minutes between notifications to the user about their play time */
	int32 NotificationRateMinutes;
	/** Reward rate at this limit */
	float RewardRate;
};

/**
 * Implementation of IOnlinePlayTimeLimit
 */
class PLAYTIMELIMIT_API FPlayTimeLimitImpl
	: public IOnlinePlayTimeLimit
{
public:
	// FPlayTimeLimitImpl

	/** Default constructor */
	FPlayTimeLimitImpl();
	/** Destructor */
	virtual ~FPlayTimeLimitImpl();

	/**
	 * Get the singleton
	 * @return Singleton instance
	 */
	static FPlayTimeLimitImpl& Get();
	
	/**
	 * Initialize
	 */
	void Initialize();

	/**
	 * Shutdown
	 */
	void Shutdown();

	/**
	 * Tick - update users and execute warn time delegates
	 */
	bool Tick(float Delta);

	DECLARE_DELEGATE_RetVal_OneParam(FPlayTimeLimitUserRawPtr, OnRequestCreateUserDelegate, const FUniqueNetId&);

	/**
	*  Delegate called when a game exit is requested
	*/
	DECLARE_MULTICAST_DELEGATE(FOnGameExitRequested);
	typedef FOnGameExitRequested::FDelegate FOnGameExitRequestedDelegate;

	/**
	 * Register a user to monitor their play time
	 * @see UnregisterUser
	 * @param NewUser the user to register
	 */
	void RegisterUser(const FUniqueNetId& NewUser);

	/**
	 * Unregister a user
	 * @see RegisterUser
	 * @param UserId the user id
	 */
	void UnregisterUser(const FUniqueNetId& UserId);

	/**
	 * Override a user's play time
	 * For testing the system without needing to potentially wait hours - waiting to accumulate time and waiting for the time to reset
	 */
	void MockUser(const FUniqueNetId& UserId, const bool bHasTimeLimit, const double CurrentPlayTimeMinutes);

	/**
	 * Cheat function to trigger the notification to players of their play time immediately
	 */
	void NotifyNow();
	
	// Begin IOnlinePlayTimeLimit
	virtual bool HasTimeLimit(const FUniqueNetId& UserId) override;
	virtual int32 GetPlayTimeMinutes(const FUniqueNetId& UserId) override;
	virtual float GetRewardRate(const FUniqueNetId& UserId) override;
	virtual FWarnUserPlayTime& GetWarnUserPlayTimeDelegate() override;
	void GameExitByRequest();
	// End IOnlinePlayTimeLimit

	/**
	 * Get the config entry that corresponds to the number of minutes played
	 * @param PlayTimeMinutes the number of minutes played to get the entry for
	 * @return the entry corresponding to the number of minutes played
	 */
	const FOnlinePlayLimitConfigEntry* GetConfigEntry(const int32 PlayTimeMinutes) const;

	/**
	 * Dump state to log
	 */
	void DumpState();

	OnRequestCreateUserDelegate OnRequestCreateUser;

	/** Delegate used to request a game exit */
	FOnGameExitRequested OnGameExitRequestedDelegate;

protected:
	/**
	 * Update the next notification time for a user based on their current play time
	 * @param User the user to update
	 */
	void UpdateNextNotificationTime(FPlayTimeLimitUser& User, const int32 PlayTimeMinutes) const;

	/** Delegate used to display a warning to the user about their play time */
	FWarnUserPlayTime WarnUserPlayTimeDelegate;

	/** List of users we are monitoring */
	TArray<FPlayTimeLimitUserPtr> Users;

	/** Last time we performed tick logic */
	double LastTickLogicTime = 0.0;

	/** Configuration to control notification rate at different levels of play time */
	TArray<FOnlinePlayLimitConfigEntry> ConfigRates;

	/** Delegate for callbacks to Tick */
	FTSTicker::FDelegateHandle TickHandle;

private:
	// Not copyable
	FPlayTimeLimitImpl(const FPlayTimeLimitImpl& Other) = delete;
	FPlayTimeLimitImpl& operator=(FPlayTimeLimitImpl& Other) = delete;
};
