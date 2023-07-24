// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Online/StatsCommon.h"

namespace UE::Online {

class FOnlineServicesNull;

class ONLINESERVICESNULL_API FStatsNull : public FStatsCommon
{
public:
	using Super = FStatsCommon;

	FStatsNull(FOnlineServicesNull& InOwningSubsystem);

	// IStats
	virtual TOnlineAsyncOpHandle<FUpdateStats> UpdateStats(FUpdateStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryStats> QueryStats(FQueryStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBatchQueryStats> BatchQueryStats(FBatchQueryStats::Params&& Params) override;
#if !UE_BUILD_SHIPPING
	virtual TOnlineAsyncOpHandle<FResetStats> ResetStats(FResetStats::Params&& Params) override;
#endif // !UE_BUILD_SHIPPING

protected:
	// TODO: Save UsersStats into local user profile
	TArray<FUserStats> UsersStats;
};

/* UE::Online */ }
