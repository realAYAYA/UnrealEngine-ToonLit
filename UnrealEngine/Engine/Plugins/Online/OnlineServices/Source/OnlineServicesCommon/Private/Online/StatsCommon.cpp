// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/StatsCommon.h"

#include "Online/Auth.h"
#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

const TCHAR* LexToString(EStatModifyMethod Value)
{
	switch (Value)
	{
	case EStatModifyMethod::Sum:
		return TEXT("Sum");
	case EStatModifyMethod::Largest:
		return TEXT("Largest");
	case EStatModifyMethod::Smallest:
		return TEXT("Smallest");
	default: checkNoEntry(); // Intentional fallthrough
	case EStatModifyMethod::Set:
		return TEXT("Set");
	}
}

void LexFromString(EStatModifyMethod& OutValue, const TCHAR* InStr)
{
	if (FCString::Stricmp(InStr, TEXT("Sum")) == 0)
	{
		OutValue = EStatModifyMethod::Sum;
	}
	else if (FCString::Stricmp(InStr, TEXT("Set")) == 0)
	{
		OutValue = EStatModifyMethod::Set;
	}
	else if (FCString::Stricmp(InStr, TEXT("Largest")) == 0)
	{
		OutValue = EStatModifyMethod::Largest;
	}
	else if (FCString::Stricmp(InStr, TEXT("Smallest")) == 0)
	{
		OutValue = EStatModifyMethod::Smallest;
	}
	else
	{
		ensureMsgf(false, TEXT("Can't convert %s to EStatModifyMethod"), InStr);
		OutValue = EStatModifyMethod::Set;
	}
}

FStatsCommon::FStatsCommon(FOnlineServicesCommon& InServices)
	: TOnlineComponent(TEXT("Stats"), InServices)
{
}

void FStatsCommon::UpdateConfig()
{
	FStatsCommonConfig Config;
	TOnlineComponent::LoadConfig(Config);

	for (FStatDefinition& StatDefinition : Config.StatDefinitions)
	{
		FString StatName = StatDefinition.Name;
		StatDefinitions.Emplace(MoveTemp(StatName), MoveTemp(StatDefinition));
	}
}

void FStatsCommon::RegisterCommands()
{
	TOnlineComponent<IStats>::RegisterCommands();

	RegisterCommand(&FStatsCommon::UpdateStats);
	RegisterCommand(&FStatsCommon::QueryStats);
	RegisterCommand(&FStatsCommon::BatchQueryStats);
#if !UE_BUILD_SHIPPING
	RegisterCommand(&FStatsCommon::ResetStats);
#endif // !UE_BUILD_SHIPPING
}

TOnlineAsyncOpHandle<FUpdateStats> FStatsCommon::UpdateStats(FUpdateStats::Params&& Params)
{
	TOnlineAsyncOpRef<FUpdateStats> Operation = GetOp<FUpdateStats>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FQueryStats> FStatsCommon::QueryStats(FQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FQueryStats> Operation = GetOp<FQueryStats>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FBatchQueryStats> FStatsCommon::BatchQueryStats(FBatchQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FBatchQueryStats> Operation = GetOp<FBatchQueryStats>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

#if !UE_BUILD_SHIPPING
TOnlineAsyncOpHandle<FResetStats> FStatsCommon::ResetStats(FResetStats::Params&& Params)
{
	TOnlineAsyncOpRef<FResetStats> Operation = GetOp<FResetStats>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}
#endif // !UE_BUILD_SHIPPING

TOnlineResult<FGetCachedStats> FStatsCommon::GetCachedStats(FGetCachedStats::Params&& Params) const
{
	return TOnlineResult<FGetCachedStats>({ CachedUsersStats });
}

void FStatsCommon::CacheUserStats(const FUserStats& UserStats)
{
	if (FUserStats* ExistingUserStats = CachedUsersStats.FindByPredicate(FFindUserStatsByAccountId(UserStats.AccountId)))
	{
		for (const TPair<FString, FStatValue>& StatPair : UserStats.Stats)
		{
			if (FStatValue* StatValue = ExistingUserStats->Stats.Find(StatPair.Key))
			{
				*StatValue = StatPair.Value;
			}
			else
			{
				ExistingUserStats->Stats.Add(StatPair);
			}
		}
	}
	else
	{
		CachedUsersStats.Emplace(UserStats);
	}
}

TOnlineEvent<void(const FStatsUpdated&)> FStatsCommon::OnStatsUpdated() 
{ 
	return OnStatsUpdatedEvent; 
}

const FStatDefinition* FStatsCommon::GetStatDefinition(const FString& StatName) const 
{ 
	return StatDefinitions.Find(StatName); 
}

/* UE::Online */ }
