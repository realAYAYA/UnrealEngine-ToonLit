// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/AchievementsCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_achievements_types.h"

namespace UE::Online {

class FOnlineServicesEOSGS;

class ONLINESERVICESEOSGS_API FAchievementsEOSGS : public FAchievementsCommon
{
public:
	using Super = FAchievementsCommon;

	FAchievementsEOSGS(FOnlineServicesEOSGS& InOwningSubsystem);
	virtual ~FAchievementsEOSGS() = default;

	// IOnlineComponent
	virtual void Initialize() override;
	virtual void Shutdown() override;

	// IAchievements
	virtual TOnlineAsyncOpHandle<FQueryAchievementDefinitions> QueryAchievementDefinitions(FQueryAchievementDefinitions::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementIds> GetAchievementIds(FGetAchievementIds::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementDefinition> GetAchievementDefinition(FGetAchievementDefinition::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryAchievementStates> QueryAchievementStates(FQueryAchievementStates::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementState> GetAchievementState(FGetAchievementState::Params&& Params) const override;
	virtual TOnlineAsyncOpHandle<FUnlockAchievements> UnlockAchievements(FUnlockAchievements::Params&& Params) override;

protected:
	EOS_HAchievements AchievementsHandle = nullptr;

	using FAchievementDefinitionMap = TMap<FString, FAchievementDefinition>;
	using FAchievementStateMap = TMap<FString, FAchievementState>;

	TOptional<FAchievementDefinitionMap> AchievementDefinitions;
	TMap<FAccountId, FAchievementStateMap> AchievementStates;

	EOS_NotificationId NotifyAchievementsUnlockedNotificationId = EOS_INVALID_NOTIFICATIONID;
};

/* UE::Online */ }
