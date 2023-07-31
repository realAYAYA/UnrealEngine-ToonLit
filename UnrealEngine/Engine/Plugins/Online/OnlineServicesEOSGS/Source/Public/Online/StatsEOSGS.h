// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/StatsCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_stats_types.h"

namespace UE::Online {

class FOnlineServicesEOSGS;

class ONLINESERVICESEOSGS_API FStatsEOSGS : public FStatsCommon
{
public:
	using Super = FStatsCommon;

	FStatsEOSGS(FOnlineServicesEOSGS& InOwningSubsystem);

	// TOnlineComponent
	virtual void Initialize() override;

	// IStats
	virtual TOnlineAsyncOpHandle<FUpdateStats> UpdateStats(FUpdateStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryStats> QueryStats(FQueryStats::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBatchQueryStats> BatchQueryStats(FBatchQueryStats::Params&& Params) override;

private:
	EOS_HStats StatsHandle = nullptr;
	TArray<FUserStats> BatchQueriedUsersStats;
};

/* UE::Online */ }
