// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/LeaderboardsCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_leaderboards_types.h"

namespace UE::Online {

class FOnlineServicesEOSGS;

class ONLINESERVICESEOSGS_API FLeaderboardsEOSGS : public FLeaderboardsCommon
{
public:
	using Super = FLeaderboardsCommon;

	FLeaderboardsEOSGS(FOnlineServicesEOSGS& InOwningSubsystem);

	// TOnlineComponent
	virtual void Initialize() override;

	// ILeaderboards
	virtual TOnlineAsyncOpHandle<FReadEntriesForUsers> ReadEntriesForUsers(FReadEntriesForUsers::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FReadEntriesAroundRank> ReadEntriesAroundRank(FReadEntriesAroundRank::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FReadEntriesAroundUser> ReadEntriesAroundUser(FReadEntriesAroundUser::Params&& Params) override;

private:
	EOS_HLeaderboards LeaderboardsHandle = nullptr;
};

/* UE::Online */ }
