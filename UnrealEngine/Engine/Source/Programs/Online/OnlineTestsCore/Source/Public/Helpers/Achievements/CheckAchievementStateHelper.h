// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Online/Achievements.h"
#include "OnlineCatchHelper.h"

namespace UE::Online
{
	inline void CheckAchievementState_Fixture(FAsyncLambdaResult Promise, SubsystemType Services, const FAccountId& AccountId, const FString& AchievementId, float AchievementProgress)
	{
		FQueryAchievementStates::Params Params{ AccountId };
		Services->GetAchievementsInterface()->QueryAchievementStates(MoveTemp(Params)).OnComplete([&](const TOnlineResult<UE::Online::FQueryAchievementStates>& Result) mutable
			{
				CHECK_OP(Result);
				FGetAchievementState::Params AchievementParams{ AccountId, AchievementId };
				TOnlineResult<FGetAchievementState> GetAchievementStateResult = Services->GetAchievementsInterface()->GetAchievementState(MoveTemp(AchievementParams));
				REQUIRE_OP(GetAchievementStateResult);
				CHECK(FMath::IsNearlyEqual(GetAchievementStateResult.GetOkValue().AchievementState.Progress, AchievementProgress));
				Promise->SetValue(true);
			});
	}
}
