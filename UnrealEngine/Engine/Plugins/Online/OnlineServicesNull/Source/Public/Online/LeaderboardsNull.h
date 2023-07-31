// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Online/LeaderboardsCommon.h"
#include "Containers/List.h"

namespace UE::Online {

class FOnlineServicesNull;

struct FUserScoreNull
{
	FAccountId AccountId;
	uint64 Score;
};

struct FLeaderboardDataNull
{
	FString Name;
	TDoubleLinkedList<FUserScoreNull> UserScoreList;
};

class ONLINESERVICESNULL_API FLeaderboardsNull : public FLeaderboardsCommon
{
public:
	using Super = FLeaderboardsCommon;

	FLeaderboardsNull(FOnlineServicesNull& InOwningSubsystem);

	// ILeaderboards
	virtual TOnlineAsyncOpHandle<FReadEntriesForUsers> ReadEntriesForUsers(FReadEntriesForUsers::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FReadEntriesAroundRank> ReadEntriesAroundRank(FReadEntriesAroundRank::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FReadEntriesAroundUser> ReadEntriesAroundUser(FReadEntriesAroundUser::Params&& Params) override;

	// FLeaderboardsCommon
	virtual TOnlineAsyncOpHandle<FWriteLeaderboardScores> WriteLeaderboardScores(FWriteLeaderboardScores::Params&& Params) override;

protected:
	TArray<FLeaderboardDataNull> LeaderboardsData;
};

/* UE::Online */ }
