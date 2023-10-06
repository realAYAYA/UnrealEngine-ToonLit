// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "OnlineIdentityInterfaceGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/achievement_manager.h"
THIRD_PARTY_INCLUDES_END

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayQueryAchievements : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayQueryAchievements(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const FUniqueNetIdGooglePlay& InUserId,
		const FOnQueryAchievementsCompleteDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryAchievements"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FUniqueNetIdGooglePlayRef UserId;
	FOnQueryAchievementsCompleteDelegate Delegate;
	gpg::AchievementManager::FetchAllResponse Response;
};
