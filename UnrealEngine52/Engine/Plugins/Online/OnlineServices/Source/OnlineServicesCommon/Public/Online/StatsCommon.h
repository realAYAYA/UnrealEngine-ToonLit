// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Stats.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

enum class EStatModifyMethod : uint8
{
	/** Add the new value to the previous value */
	Sum,
	/** Overwrite previous value with the new value */
	Set,
	/** Only replace previous value if new value is larger */
	Largest,
	/** Only replace previous value if new value is smaller */
	Smallest
};

const TCHAR* LexToString(EStatModifyMethod Value);
void LexFromString(EStatModifyMethod& OutValue, const TCHAR* InStr);

struct FStatDefinition
{
	/* The name of the stat */
	FString Name;
	/* Corresponding stat id on the platform if needed */
	int32 Id = 0;
	/* How the stat will be modified */
	EStatModifyMethod ModifyMethod = EStatModifyMethod::Set;
};

struct FStatsCommonConfig
{
	TArray<FStatDefinition> StatDefinitions;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FStatDefinition)
	ONLINE_STRUCT_FIELD(FStatDefinition, Name),
	ONLINE_STRUCT_FIELD(FStatDefinition, Id),
	ONLINE_STRUCT_FIELD(FStatDefinition, ModifyMethod)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FStatsCommonConfig)
	ONLINE_STRUCT_FIELD(FStatsCommonConfig, StatDefinitions)
END_ONLINE_STRUCT_META()

/* Meta */ }

struct FFindUserStatsByAccountId
{
	FFindUserStatsByAccountId(const FAccountId& InAccountId)
		: AccountId(InAccountId)
	{
	}

	bool operator()(const FUserStats& UserStats) const
	{
		return UserStats.AccountId == AccountId;
	}

	FAccountId AccountId;
};

class ONLINESERVICESCOMMON_API FStatsCommon : public TOnlineComponent<IStats>
{
public:
	using Super = IStats;

	FStatsCommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void UpdateConfig() override;
	virtual void RegisterCommands() override;

	// IStats
	virtual TOnlineAsyncOpHandle<FUpdateStats> UpdateStats(FUpdateStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryStats> QueryStats(FQueryStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBatchQueryStats> BatchQueryStats(FBatchQueryStats::Params&& Params) override;
#if !UE_BUILD_SHIPPING
	virtual TOnlineAsyncOpHandle<FResetStats> ResetStats(FResetStats::Params&& Params) override;
#endif // !UE_BUILD_SHIPPING
	virtual TOnlineResult<FGetCachedStats> GetCachedStats(FGetCachedStats::Params&& Params) const override;

	virtual TOnlineEvent<void(const FStatsUpdated&)> OnStatsUpdated() override;
	const FStatDefinition* GetStatDefinition(const FString& StatName) const;

protected:
	void CacheUserStats(const FUserStats& UserStats);

	TMap<FString, FStatDefinition> StatDefinitions;
	TOnlineEventCallable<void(const FStatsUpdated& StatsUpdated)> OnStatsUpdatedEvent;

	TArray<FUserStats> CachedUsersStats;
};

/* UE::Online */ }
