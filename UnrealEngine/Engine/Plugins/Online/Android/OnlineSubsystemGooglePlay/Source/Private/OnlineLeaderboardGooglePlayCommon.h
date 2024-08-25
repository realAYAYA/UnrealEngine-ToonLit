// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FGooglePlayLeaderboardScore
{
	// Leaderboard ID as shown in GooglePlay Console
	FString GooglePlayLeaderboardId;
	// Leaderboard score
	int64 Score = 0;
};
