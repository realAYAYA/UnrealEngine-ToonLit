// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Achievements.h"
#include "Online/OnlineComponent.h"
#include "Online/Stats.h"

namespace UE::Online {

class FOnlineServicesCommon;

struct FAchievementUnlockCondition
{
	FString StatName;
	FStatValue UnlockThreshold; // The unlock rule depends on Stat modification type
};

struct FAchievementUnlockRule
{
	FString AchievementId;
	TArray<FAchievementUnlockCondition> Conditions;

	bool ContainsStat(const FString& StatName) const;
};

struct FAchievementsCommonConfig
{
	bool bIsTitleManaged = false;
	TArray<FAchievementUnlockRule> UnlockRules;
};

namespace Meta
{

BEGIN_ONLINE_STRUCT_META(FAchievementUnlockCondition)
	ONLINE_STRUCT_FIELD(FAchievementUnlockCondition, StatName),
	ONLINE_STRUCT_FIELD(FAchievementUnlockCondition, UnlockThreshold)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAchievementUnlockRule)
	ONLINE_STRUCT_FIELD(FAchievementUnlockRule, AchievementId),
	ONLINE_STRUCT_FIELD(FAchievementUnlockRule, Conditions)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAchievementsCommonConfig)
	ONLINE_STRUCT_FIELD(FAchievementsCommonConfig, bIsTitleManaged),
	ONLINE_STRUCT_FIELD(FAchievementsCommonConfig, UnlockRules)
END_ONLINE_STRUCT_META()

/* Meta */ }

class ONLINESERVICESCOMMON_API FAchievementsCommon : public TOnlineComponent<IAchievements>
{
public:
	using Super = IAchievements;

	FAchievementsCommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void UpdateConfig() override;
	virtual void RegisterCommands() override;

	// IAchievements
	virtual TOnlineAsyncOpHandle<FQueryAchievementDefinitions> QueryAchievementDefinitions(FQueryAchievementDefinitions::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementIds> GetAchievementIds(FGetAchievementIds::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementDefinition> GetAchievementDefinition(FGetAchievementDefinition::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryAchievementStates> QueryAchievementStates(FQueryAchievementStates::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementState> GetAchievementState(FGetAchievementState::Params&& Params) const override;
	virtual TOnlineAsyncOpHandle<FUnlockAchievements> UnlockAchievements(FUnlockAchievements::Params&& Params) override;
	virtual TOnlineResult<FDisplayAchievementUI> DisplayAchievementUI(FDisplayAchievementUI::Params&& Params) override;
	virtual TOnlineEvent<void(const FAchievementStateUpdated&)> OnAchievementStateUpdated() override;

protected:
	TOnlineEventCallable<void(const FAchievementStateUpdated&)> OnAchievementStateUpdatedEvent;

	void UnlockAchievementsByStats(const FStatsUpdated& StatsUpdated);
	void ExecuteUnlockRulesRelatedToStat(const FAccountId& AccountId, const FString& StatName, const TMap<FString, FStatValue>& Stats, TArray<FString>& OutAchievementsToUnlock);
	bool MeetUnlockCondition(const FAchievementUnlockRule& AchievementUnlockRule, const TMap<FString, FStatValue>& Stats);
	bool IsUnlocked(const FAccountId& AccountId, const FString& AchievementName) const;

	FOnlineEventDelegateHandle StatEventHandle;

	FAchievementsCommonConfig Config;
};

/* UE::Online */ }
