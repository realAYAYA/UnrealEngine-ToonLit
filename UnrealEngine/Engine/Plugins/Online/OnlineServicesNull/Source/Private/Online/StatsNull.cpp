// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/StatsNull.h"
#include "Online/AuthNull.h"
#include "Online/OnlineServicesNull.h"
#include "Math/UnrealMathUtility.h"

namespace UE::Online {

FStatsNull::FStatsNull(FOnlineServicesNull& InOwningSubsystem)
	: Super(InOwningSubsystem)
{
}

TOnlineAsyncOpHandle<FUpdateStats> FStatsNull::UpdateStats(FUpdateStats::Params&& Params)
{
	TOnlineAsyncOpRef<FUpdateStats> Op = GetOp<FUpdateStats>(MoveTemp(Params));

	if (!Services.Get<FAuthNull>()->IsLoggedIn(Op->GetParams().LocalAccountId))
	{
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	Op->Then([this](TOnlineAsyncOp<FUpdateStats>& InAsyncOp) mutable
	{
		for (const FUserStats& UpdateUserStats : InAsyncOp.GetParams().UpdateUsersStats)
		{
			FUserStats* ExistingUserStats = UsersStats.FindByPredicate(FFindUserStatsByAccountId(UpdateUserStats.AccountId));
			if (!ExistingUserStats)
			{
				ExistingUserStats = &UsersStats.Emplace_GetRef();
				ExistingUserStats->AccountId = UpdateUserStats.AccountId;
			}

			TMap<FString, FStatValue>& UserStats = ExistingUserStats->Stats;
			for (const auto& UpdateUserStatPair : UpdateUserStats.Stats)
			{
				if (const FStatDefinition* StatDefinition = GetStatDefinition(UpdateUserStatPair.Key))
				{
					if (FStatValue* StatValue = UserStats.Find(UpdateUserStatPair.Key))
					{
						switch (StatDefinition->ModifyMethod)
						{
						case EStatModifyMethod::Set:
							*StatValue = UpdateUserStatPair.Value;
							break;
						case EStatModifyMethod::Sum:
							if (StatValue->VariantData.IsType<double>())
							{
								StatValue->Set(StatValue->GetDouble() + UpdateUserStatPair.Value.GetDouble());
							}
							else if (StatValue->VariantData.IsType<int64>())
							{
								StatValue->Set(StatValue->GetInt64() + UpdateUserStatPair.Value.GetInt64());
							}
							break;
						case EStatModifyMethod::Largest:
							if (StatValue->VariantData.IsType<double>())
							{
								StatValue->Set(FMath::Max(StatValue->GetDouble(), UpdateUserStatPair.Value.GetDouble()));
							}
							else if (StatValue->VariantData.IsType<int64>())
							{
								StatValue->Set(FMath::Max(StatValue->GetInt64(), UpdateUserStatPair.Value.GetInt64()));
							}
							break;
						case EStatModifyMethod::Smallest:
							if (StatValue->VariantData.IsType<double>())
							{
								StatValue->Set(FMath::Min(StatValue->GetDouble(), UpdateUserStatPair.Value.GetDouble()));
							}
							else if (StatValue->VariantData.IsType<int64>())
							{
								StatValue->Set(FMath::Min(StatValue->GetInt64(), UpdateUserStatPair.Value.GetInt64()));
							}
							break;
						}
					}
					else
					{
						UserStats.Emplace(UpdateUserStatPair.Key, UpdateUserStatPair.Value);
					}
				}
			}

			CacheUserStats(*ExistingUserStats);
		}

		InAsyncOp.SetResult({});
		OnStatsUpdatedEvent.Broadcast(InAsyncOp.GetParams());
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FQueryStats> FStatsNull::QueryStats(FQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FQueryStats> Op = GetOp<FQueryStats>(MoveTemp(Params));

	if (!Services.Get<FAuthNull>()->IsLoggedIn(Op->GetParams().LocalAccountId))
	{
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	Op->Then([this](TOnlineAsyncOp<FQueryStats>& InAsyncOp)
	{
		FQueryStats::Result Result;

		if (FUserStats* ExistingUserStats = UsersStats.FindByPredicate(FFindUserStatsByAccountId(InAsyncOp.GetParams().TargetAccountId)))
		{
			Result.Stats = ExistingUserStats->Stats;
		}

		InAsyncOp.SetResult(MoveTemp(Result));
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FBatchQueryStats> FStatsNull::BatchQueryStats(FBatchQueryStats::Params&& Params)
{
	TOnlineAsyncOpRef<FBatchQueryStats> Op = GetOp<FBatchQueryStats>(MoveTemp(Params));
	if (!Services.Get<FAuthNull>()->IsLoggedIn(Op->GetParams().LocalAccountId))
	{
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	Op->Then([this](TOnlineAsyncOp<FBatchQueryStats>& InAsyncOp)
	{
		FBatchQueryStats::Result Result;

		for (const FAccountId& TargetAccountId : InAsyncOp.GetParams().TargetAccountIds)
		{
			if (FUserStats* ExistingUserStats = UsersStats.FindByPredicate(FFindUserStatsByAccountId(TargetAccountId)))
			{
				Result.UsersStats.Emplace(*ExistingUserStats);
			}
		}

		InAsyncOp.SetResult(MoveTemp(Result));
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

#if !UE_BUILD_SHIPPING
TOnlineAsyncOpHandle<FResetStats> FStatsNull::ResetStats(FResetStats::Params&& Params)
{
	TOnlineAsyncOpRef<FResetStats> Op = GetOp<FResetStats>(MoveTemp(Params));

	if (!Services.Get<FAuthNull>()->IsLoggedIn(Op->GetParams().LocalAccountId))
	{
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	Op->Then([this](TOnlineAsyncOp<FResetStats>& InAsyncOp)
	{
		uint32 Index = UsersStats.IndexOfByPredicate(FFindUserStatsByAccountId(InAsyncOp.GetParams().LocalAccountId));
		if (Index != INDEX_NONE)
		{
			UsersStats.RemoveAt(Index);
		}

		FFindUserStatsByAccountId FindUserStatsByAccountId(InAsyncOp.GetParams().LocalAccountId);

		UsersStats.RemoveAll(FindUserStatsByAccountId);
		CachedUsersStats.RemoveAll(FindUserStatsByAccountId);

		InAsyncOp.SetResult({});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}
#endif // !UE_BUILD_SHIPPING

/* UE::Online */ }
