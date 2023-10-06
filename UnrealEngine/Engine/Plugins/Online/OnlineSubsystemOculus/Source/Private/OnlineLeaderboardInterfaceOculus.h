// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineLeaderboardInterface.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

/**
*	IOnlineLeaderboard - Interface class for Leaderboard
*/
class FOnlineLeaderboardOculus : public IOnlineLeaderboards
{
private:
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Reference to the owning subsystem */
	FOnlineSubsystemOculus& OculusSubsystem;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	bool ReadOculusLeaderboards(bool bOnlyFriends, bool bOnlyLoggedInUser, FOnlineLeaderboardReadRef& ReadObject);
	void OnReadLeaderboardsComplete(ovrMessageHandle Message, bool bIsError, const FOnlineLeaderboardReadRef& ReadObject);

public:

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	* Constructor
	*
	* @param InSubsystem - A reference to the owning subsystem
	*/
	FOnlineLeaderboardOculus(FOnlineSubsystemOculus& InSubsystem);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	* Default destructor
	*/
	virtual ~FOnlineLeaderboardOculus() = default;

	// Begin IOnlineLeaderboard interface
	virtual bool ReadLeaderboards(const TArray< FUniqueNetIdRef >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(FUniqueNetIdRef Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;
	// End IOnlineLeaderboard interface
};
