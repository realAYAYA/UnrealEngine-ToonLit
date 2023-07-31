// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Online/AchievementsCommon.h"

namespace UE::Online {

class FOnlineServicesNull;

struct FAchievementsNullConfig
{
	TArray<FAchievementDefinition> AchievementDefinitions;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAchievementsNullConfig)
	ONLINE_STRUCT_FIELD(FAchievementsNullConfig, AchievementDefinitions)
END_ONLINE_STRUCT_META()

/* Meta */ }

class ONLINESERVICESNULL_API FAchievementsNull : public FAchievementsCommon
{
public:
	using Super = FAchievementsCommon;

	FAchievementsNull(FOnlineServicesNull& InOwningSubsystem);

	// IOnlineComponent
	virtual void UpdateConfig() override;

	// IAchievements
	virtual TOnlineAsyncOpHandle<FQueryAchievementDefinitions> QueryAchievementDefinitions(FQueryAchievementDefinitions::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementIds> GetAchievementIds(FGetAchievementIds::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementDefinition> GetAchievementDefinition(FGetAchievementDefinition::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryAchievementStates> QueryAchievementStates(FQueryAchievementStates::Params&& Params) override;
	virtual TOnlineResult<FGetAchievementState> GetAchievementState(FGetAchievementState::Params&& Params) const override;
	virtual TOnlineAsyncOpHandle<FUnlockAchievements> UnlockAchievements(FUnlockAchievements::Params&& Params) override;
	virtual TOnlineResult<FDisplayAchievementUI> DisplayAchievementUI(FDisplayAchievementUI::Params&& Params) override;

protected:
	using FAchievementDefinitionMap = TMap<FString, FAchievementDefinition>;
	using FAchievementStateMap = TMap<FString, FAchievementState>;

	bool bAchievementDefinitionsQueried = false;
	TMap<FAccountId, FAchievementStateMap> AchievementStates;

	FAchievementsNullConfig Config;

	const FAchievementDefinition* FindAchievementDefinition(const FString& AchievementId) const;
};

/* UE::Online */ }
