// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Achievements/QueryAchievementDefinitionsHelper.h"
#include "Helpers/Identity/IdentityGetLoginByUserId.h"
#include "Helpers/LambdaStep.h"
#include "OnlineCatchHelper.h"


#define ACHIEVEMENTS_TAG "[suite_achievements]"
#define EG_ACHIEVEMENTS_GETACHIEVEMENTDEFINITION_TAG ACHIEVEMENTS_TAG "[getachievementdefinition]"

#define ACHIEVEMENTS_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, ACHIEVEMENTS_TAG __VA_ARGS__)


ACHIEVEMENTS_TEST_CASE("Get Achievement Definition (Invalid User)", EG_ACHIEVEMENTS_GETACHIEVEMENTDEFINITION_TAG)
{
	int32 NumUsersToImplicitLogin = 0;

	GetLoginPipeline(NumUsersToImplicitLogin).EmplaceLambda([](SubsystemType OnlineSubsystem)
		{
			UE::Online::FGetAchievementDefinition::Params OpParams;

			UE::Online::IAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
			UE::Online::TOnlineResult<UE::Online::FGetAchievementDefinition> Result = AchievementsInterface->GetAchievementDefinition(MoveTemp(OpParams));
			REQUIRE(Result.IsError());
			CHECK(Result.GetErrorValue() == UE::Online::Errors::InvalidUser());
		});

	RunToCompletion();
}

ACHIEVEMENTS_TEST_CASE("Get Achievement Definition (Invalid State)", EG_ACHIEVEMENTS_GETACHIEVEMENTDEFINITION_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId AccountId;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	UE::Online::FGetAchievementDefinition::Params GetAchievementDefinitionParams;
	GetAchievementDefinitionParams.LocalAccountId = AccountId;

	LoginPipeline.EmplaceLambda([&GetAchievementDefinitionParams](SubsystemType OnlineSubsystem)
		{
			UE::Online::IAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
			const UE::Online::TOnlineResult<UE::Online::FGetAchievementDefinition> Result = AchievementsInterface->GetAchievementDefinition(MoveTemp(GetAchievementDefinitionParams));
			REQUIRE(Result.IsError());
			CHECK(Result.GetErrorValue() == UE::Online::Errors::InvalidState());
		});

	RunToCompletion();
}

ACHIEVEMENTS_TEST_CASE("Get Achievement Definition (Not Found)", EG_ACHIEVEMENTS_GETACHIEVEMENTDEFINITION_TAG)
{
	FAccountId AccountId;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	FQueryAchievementDefinitionsHelper::FHelperParams QueryDefinitionsHelperParams;
	QueryDefinitionsHelperParams.OpParams.LocalAccountId = AccountId;

	UE::Online::FGetAchievementDefinition::Params GetParams;
	GetParams.LocalAccountId = AccountId;
	GetParams.AchievementId = TEXT("UnknownAchievement");

	LoginPipeline.EmplaceStep<FQueryAchievementDefinitionsHelper>(MoveTemp(QueryDefinitionsHelperParams))
		.EmplaceLambda([&GetParams](SubsystemType OnlineSubsystem)
		{
			UE::Online::IAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
			const UE::Online::TOnlineResult<UE::Online::FGetAchievementDefinition> Result = AchievementsInterface->GetAchievementDefinition(MoveTemp(GetParams));
			REQUIRE(Result.IsError());
			CHECK(Result.GetErrorValue() == UE::Online::Errors::NotFound());
		});

	RunToCompletion();
}

ACHIEVEMENTS_TEST_CASE("Get Achievement Definitions (Success)", EG_ACHIEVEMENTS_GETACHIEVEMENTDEFINITION_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId AccountId;
	TArray<FString> AchievementIds;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	FQueryAchievementDefinitionsHelper::FHelperParams QueryDefinitionsHelperParams;
	QueryDefinitionsHelperParams.OpParams.LocalAccountId = AccountId;

	UE::Online::FGetAchievementIds::Params GetParams;
	GetParams.LocalAccountId = AccountId;

	LoginPipeline.EmplaceStep<FQueryAchievementDefinitionsHelper>(MoveTemp(QueryDefinitionsHelperParams))
		.EmplaceLambda([&GetParams, &AccountId, &AchievementIds](SubsystemType OnlineSubsystem)
		{
			UE::Online::IAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
			UE::Online::TOnlineResult<UE::Online::FGetAchievementIds> Result = AchievementsInterface->GetAchievementIds(MoveTemp(GetParams));
			REQUIRE_OP(Result);
			AchievementIds = MoveTemp(Result.GetOkValue().AchievementIds);
		})
		.EmplaceLambda([&AccountId, &AchievementIds](SubsystemType OnlineSubsystem)
		{
			REQUIRE(!AchievementIds.IsEmpty());
			for (const FString& AchievementId : AchievementIds)
			{
				UE::Online::FGetAchievementDefinition::Params GetParams;
				GetParams.LocalAccountId = AccountId;
				GetParams.AchievementId = AchievementId;

				UE::Online::IAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
				const UE::Online::TOnlineResult<UE::Online::FGetAchievementDefinition> Result = AchievementsInterface->GetAchievementDefinition(MoveTemp(GetParams));
				REQUIRE_OP(Result);
			}
		});

	RunToCompletion();
}