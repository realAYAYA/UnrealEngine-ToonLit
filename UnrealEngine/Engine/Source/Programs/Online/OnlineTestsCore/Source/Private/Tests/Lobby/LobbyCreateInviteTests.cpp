// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include <catch2/catch_test_macros.hpp>

#include "Helpers/Identity/IdentityGetLoginByUserId.h"
#include "Helpers/Lobby/LobbyCreateHelper.h"
#include "OnlineCatchHelper.h"

#define LOBBY_CREATE_INVITE_TAGS "[Lobby]"
#define LOBBY_CREATE_INVITE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, LOBBY_CREATE_INVITE_TAGS __VA_ARGS__)

LOBBY_CREATE_INVITE_TEST_CASE("Basic lobby create test")
{
	int32 LocalUserNum = 0;
	UE::Online::FCreateLobby::Params Params;
	Params.LocalName = TEXT("TestLobby");
	Params.SchemaId = TEXT("test");
	Params.MaxMembers = 2;
	Params.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;
	Params.Attributes = { { TEXT("LobbyCreateTime"), (int64)10}};

	GetLoginPipeline(1)
		.EmplaceStep<FIdentityGetUniquePlayerIdStep>(LocalUserNum, 
			[&Params](UE::Online::FAccountInfo InAccountInfo) {
				Params.LocalAccountId = InAccountInfo.AccountId;
			})
		.EmplaceStep<FLobbyCreateHelper>(&Params, true);

	RunToCompletion();
}
