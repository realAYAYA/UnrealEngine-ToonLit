// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/StatsEOSGS.h"
#include "EOSShared.h"
#include "Online/AuthEOSGS.h"
#include "Online/OnlineErrorEOSGS.h"
#include "Online/OnlineIdEOSGS.h"
#include "Online/OnlineServicesEOSGS.h"

#include "eos_stats.h"

namespace UE::Online {

#define STAT_NAME_MAX_LENGTH_EOS 256 + 1 // 256 plus null terminator

struct FStatNameRawBufferEOS
{
	char StatName[STAT_NAME_MAX_LENGTH_EOS];
};

FStatsEOSGS::FStatsEOSGS(FOnlineServicesEOSGS& InServices)
	: Super(InServices)
{
}

void FStatsEOSGS::Initialize()
{
	Super::Initialize();

	StatsHandle = EOS_Platform_GetStatsInterface(static_cast<FOnlineServicesEOSGS&>(GetServices()).GetEOSPlatformHandle());
	check(StatsHandle);
}

TOnlineAsyncOpHandle<FUpdateStats> FStatsEOSGS::UpdateStats(FUpdateStats::Params&& Params)
{
	TOnlineAsyncOpRef<FUpdateStats> Op = GetOp<FUpdateStats>(MoveTemp(Params));

	if (!Services.Get<FAuthEOSGS>()->IsLoggedIn(Op->GetParams().LocalAccountId))
	{
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	for (const FUserStats& UpdateUserStats : Op->GetParams().UpdateUsersStats)
	{
		if (Op->GetParams().LocalAccountId != UpdateUserStats.AccountId)
		{
			// Client can only update the stats of the local account itself
			Op->SetError(Errors::InvalidParams());
			return Op->GetHandle();
		}
	}

	for (const FUserStats& UpdateUserStats : Op->GetParams().UpdateUsersStats)
	{
		Op->Then([this, &UpdateUserStats](TOnlineAsyncOp<FUpdateStats>& InAsyncOp, TPromise<const EOS_Stats_IngestStatCompleteCallbackInfo*>&& Promise)
		{
			TArray<EOS_Stats_IngestData> EOSData;
			TArray<FStatNameRawBufferEOS> EOSStatNames;

			// Preallocate all of the memory
			EOSData.AddZeroed(UpdateUserStats.Stats.Num());
			EOSStatNames.AddZeroed(UpdateUserStats.Stats.Num());

			uint32 Index = 0;
			for (const TPair<FString, FStatValue>& StatPair : UpdateUserStats.Stats)
			{
				EOS_Stats_IngestData& EOSStat = EOSData[Index];
				EOSStat.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_STATS_INGESTDATA_API_LATEST, 1);

				EOSStat.IngestAmount = static_cast<int32>(StatPair.Value.GetInt64());
				FCStringAnsi::Strncpy(EOSStatNames[Index].StatName, TCHAR_TO_UTF8(*StatPair.Key), STAT_NAME_MAX_LENGTH_EOS);
				EOSStat.StatName = EOSStatNames[Index].StatName;

				Index++;
			}

			EOS_Stats_IngestStatOptions Options = { };
			Options.ApiVersion = 3;
			UE_EOS_CHECK_API_MISMATCH(EOS_STATS_INGESTSTAT_API_LATEST, 3);
			Options.LocalUserId = GetProductUserIdChecked(InAsyncOp.GetParams().LocalAccountId);
			Options.TargetUserId = GetProductUserIdChecked(UpdateUserStats.AccountId);
			Options.Stats = EOSData.GetData();
			Options.StatsCount = EOSData.Num();

			EOS_Async(EOS_Stats_IngestStat, StatsHandle, Options, MoveTemp(Promise));
		})
		.Then([this](TOnlineAsyncOp<FUpdateStats>& InAsyncOp, const EOS_Stats_IngestStatCompleteCallbackInfo* Data)
		{
			if (Data->ResultCode != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("EOS_Stats_IngestStat failed with result=[%s]"), *LexToString(Data->ResultCode));
				InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
				return;
			}
		});
	}

	Op->Then([this](TOnlineAsyncOp<FUpdateStats>& InAsyncOp)
	{
		InAsyncOp.SetResult({});
		OnStatsUpdatedEvent.Broadcast(InAsyncOp.GetParams());
	});

	Op->Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

namespace Private
{

void QueryStatsEOS(EOS_HStats StatsHandle, const FAccountId& LocalAccountId, const FAccountId& TargetAccountId, const TArray<FString>& StatNames, TPromise<const EOS_Stats_OnQueryStatsCompleteCallbackInfo*>&& Promise)
{
	EOS_Stats_QueryStatsOptions Options;
	Options.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_STATS_INGESTSTAT_API_LATEST, 3);
	Options.StartTime = EOS_STATS_TIME_UNDEFINED;
	Options.EndTime = EOS_STATS_TIME_UNDEFINED;

	TArray<FStatNameRawBufferEOS> EOSStatNames;
	TArray<char*> EOSStatNamesPtr;

	EOSStatNames.AddZeroed(StatNames.Num());
	EOSStatNamesPtr.AddZeroed(StatNames.Num());

	uint32 Index = 0;
	for (const FString& StatName : StatNames)
	{
		FCStringAnsi::Strncpy(EOSStatNames[Index].StatName, TCHAR_TO_UTF8(*StatName), STAT_NAME_MAX_LENGTH_EOS);
		EOSStatNamesPtr[Index] = EOSStatNames[Index].StatName;
		Index++;
	}

	Options.StatNames = (const char**)EOSStatNamesPtr.GetData();
	Options.StatNamesCount = StatNames.Num();
	Options.LocalUserId = GetProductUserIdChecked(LocalAccountId);
	Options.TargetUserId = GetProductUserIdChecked(TargetAccountId);

	EOS_Async(EOS_Stats_QueryStats, StatsHandle, Options, MoveTemp(Promise));
}

void ReadStatsFromEOSResult(EOS_HStats StatsHandle, const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data, const TArray<FString>& StatNames, TMap<FString, FStatValue>& OutStats)
{
	for (const FString& StatName : StatNames)
	{
		char StatNameANSI[STAT_NAME_MAX_LENGTH_EOS];
		EOS_Stats_CopyStatByNameOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_STATS_COPYSTATBYNAME_API_LATEST, 1);
		Options.TargetUserId = Data->TargetUserId;
		Options.Name = StatNameANSI;
		FCStringAnsi::Strncpy(StatNameANSI, TCHAR_TO_UTF8(*StatName), STAT_NAME_MAX_LENGTH_EOS);

		EOS_Stats_Stat* ReadStat = nullptr;
		if (EOS_Stats_CopyStatByName(StatsHandle, &Options, &ReadStat) == EOS_EResult::EOS_Success)
		{
			OutStats.Add(StatName, FStatValue(static_cast<int64>(ReadStat->Value)));
			EOS_Stats_Stat_Release(ReadStat);
		}
		else
		{
			// Put an empty stat in
			UE_LOG(LogTemp, VeryVerbose, TEXT("Value not found for stat %s, adding empty value"), *StatName);
			OutStats.Add(StatName, FStatValue(static_cast<int64>(0)));
		}
	}
}

}

TOnlineAsyncOpHandle<FQueryStats> FStatsEOSGS::QueryStats(FQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FQueryStats> Op = GetJoinableOp<FQueryStats>(MoveTemp(Params));
	if (!Op->IsReady())
	{
		Op->Then([this](TOnlineAsyncOp<FQueryStats>& InAsyncOp)
		{
			const FQueryStats::Params& Params = InAsyncOp.GetParams();
			if (!Services.Get<FAuthEOSGS>()->IsLoggedIn(Params.LocalAccountId))
			{
				InAsyncOp.SetError(Errors::InvalidUser());
			}
		})
		.Then([this](TOnlineAsyncOp<FQueryStats>& InAsyncOp, TPromise<const EOS_Stats_OnQueryStatsCompleteCallbackInfo*>&& Promise)
		{
			const FQueryStats::Params& Params = InAsyncOp.GetParams();
			Private::QueryStatsEOS(StatsHandle, Params.LocalAccountId, Params.TargetAccountId, Params.StatNames, MoveTemp(Promise));
		})
		.Then([this](TOnlineAsyncOp<FQueryStats>& InAsyncOp, const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data)
		{
			if (Data->ResultCode != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("EOS_Stats_QueryStats failed with result=[%s]"), *LexToString(Data->ResultCode));
				InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
				return;
			}

			const FQueryStats::Params& Params = InAsyncOp.GetParams();

			FQueryStats::Result Result;
			Private::ReadStatsFromEOSResult(StatsHandle, Data, Params.StatNames, Result.Stats);

			FUserStats UserStats;
			UserStats.AccountId = Params.TargetAccountId;
			UserStats.Stats = Result.Stats;
			CacheUserStats(UserStats);

			InAsyncOp.SetResult(MoveTemp(Result));
		})
		.Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FBatchQueryStats> FStatsEOSGS::BatchQueryStats(FBatchQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FBatchQueryStats> Op = GetJoinableOp<FBatchQueryStats>(MoveTemp(Params));
	if (!Op->IsReady())
	{
		Op->Then([this](TOnlineAsyncOp<FBatchQueryStats>& InAsyncOp)
		{
			const FBatchQueryStats::Params& Params = InAsyncOp.GetParams();
			if (!Services.Get<FAuthEOSGS>()->IsLoggedIn(Params.LocalAccountId))
			{
				InAsyncOp.SetError(Errors::InvalidUser());
			}
			else if (Params.TargetAccountIds.IsEmpty() || Params.StatNames.IsEmpty())
			{
				InAsyncOp.SetError(Errors::InvalidParams());
			}
		});

		for (const FAccountId& TargetAccountId : Op->GetParams().TargetAccountIds)
		{
			Op->Then([this, TargetAccountId](TOnlineAsyncOp<FBatchQueryStats>& InAsyncOp, TPromise<const EOS_Stats_OnQueryStatsCompleteCallbackInfo*>&& Promise)
			{
				Private::QueryStatsEOS(StatsHandle, InAsyncOp.GetParams().LocalAccountId, TargetAccountId, InAsyncOp.GetParams().StatNames, MoveTemp(Promise));
			})
			.Then([this, TargetAccountId](TOnlineAsyncOp<FBatchQueryStats>& InAsyncOp, const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data)
			{
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOS_Stats_QueryStats failed with result=[%s]"), *LexToString(Data->ResultCode));
					BatchQueriedUsersStats.Reset();
					InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
					return;
				}

				FUserStats& UserStats = BatchQueriedUsersStats.Emplace_GetRef();
				UserStats.AccountId = TargetAccountId;

				Private::ReadStatsFromEOSResult(StatsHandle, Data, InAsyncOp.GetParams().StatNames, UserStats.Stats);
				CacheUserStats(UserStats);
			});
		}

		Op->Then([this](TOnlineAsyncOp<FBatchQueryStats>& InAsyncOp)
		{
			FBatchQueryStats::Result Result;
			Result.UsersStats = MoveTemp(BatchQueriedUsersStats);
			InAsyncOp.SetResult(MoveTemp(Result));
		})
		.Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

/* UE::Online */ }
