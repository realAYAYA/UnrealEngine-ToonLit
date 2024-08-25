// Copyright Epic Games, Inc. All Rights Reserved.

#include <catch2/catch_test_macros.hpp>

#include "Helpers/Identity/IdentityGetLoginByUserId.h"
#include "Helpers/Stats/UpdateStatsHelper.h"
#include "AsyncTestStep.h"
#include "OnlineCatchHelper.h"
#include "Online/Stats.h"
#include "Online/Leaderboards.h"


void ReadEntriesAroundUser_2UsersFixture(FAsyncLambdaResult Promise, SubsystemType Services, const FString& BoardName, const FAccountId& AccountIdA, const FAccountId& AccountIdB, int32 Offset, uint32 Limit, TArray<UE::Online::FLeaderboardEntry>& OutLeaderboardEntries);
void ReadEntriesAroundUser_5UsersFixture(FAsyncLambdaResult Promise, SubsystemType Services, const FString& BoardName, const TArray<FAccountId>& AccountIds, int32 Offset, uint32 Limit, TArray<UE::Online::FLeaderboardEntry>& OutLeaderboardEntries);
void ReadEntriesAroundUserFixture(FAsyncLambdaResult Promise, SubsystemType Services, const FString& BoardName, const FAccountId& AccountIdA, const FAccountId& AccountIdB, int32 Offset, uint32 Limit, const UE::Online::FOnlineError& InError, TArray<UE::Online::FLeaderboardEntry>& OutLeaderboardEntries);
