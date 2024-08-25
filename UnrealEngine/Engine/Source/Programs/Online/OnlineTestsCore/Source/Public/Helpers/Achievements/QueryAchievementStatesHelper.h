// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TestDriver.h"
#include "TestHarness.h"
#include "OnlineCatchStringMakers.h"

#include "Online/AuthCommon.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/Achievements.h"

struct FQueryAchievementStatesHelper : public FTestPipeline::FStep
{
	using ParamsType = UE::Online::FQueryAchievementStates::Params;
	using ResultType = UE::Online::TOnlineResult<UE::Online::FQueryAchievementStates>;

	struct FHelperParams
	{
		ParamsType OpParams;
		TOptional<ResultType> ExpectedError;
	};

	FQueryAchievementStatesHelper(FHelperParams&& InHelperParams)
		: HelperParams(MoveTemp(InHelperParams))
	{
		REQUIRE((!HelperParams.ExpectedError.IsSet() || HelperParams.ExpectedError->IsError()));
	}

	virtual ~FQueryAchievementStatesHelper() = default;

	enum class EState { Init, Called, Done } State = EState::Init;

	virtual EContinuance Tick(SubsystemType OnlineSubsystem) override
	{
		switch (State)
		{
		case EState::Init:
		{
			AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
			REQUIRE(AchievementsInterface);

			State = EState::Called;
			AchievementsInterface->QueryAchievementStates(MoveTemp(HelperParams.OpParams))
			.OnComplete([this](const ResultType& Result)
			{
				State = EState::Done;
				if (!HelperParams.ExpectedError.IsSet())
				{
					REQUIRE_OP(Result);
				}
				else
				{
					const UE::Online::FOnlineError* ErrorValue = Result.TryGetErrorValue();
					REQUIRE(ErrorValue != nullptr);
					REQUIRE(*ErrorValue == HelperParams.ExpectedError->GetErrorValue());
				}
			});
			break;
		}
		}

		return State == EState::Done ? EContinuance::Done : EContinuance::ContinueStepping;
	}

protected:
	FHelperParams HelperParams;

	UE::Online::IAchievementsPtr AchievementsInterface = nullptr;
};