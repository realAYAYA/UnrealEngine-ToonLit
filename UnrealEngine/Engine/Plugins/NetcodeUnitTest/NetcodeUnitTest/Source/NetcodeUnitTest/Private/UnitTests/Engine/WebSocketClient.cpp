// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnitTests/Engine/WebSocketClient.h"

#include "MinimalClient.h"
#include "UnitTestEnvironment.h"

/**
 * UWebSocketClient
 */

UWebSocketClient::UWebSocketClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnitTestName = TEXT("WebSocketClient");

	UnitTestDate = FDateTime(2017, 10, 5);

	UnitTestBugTrackIDs.Empty();

	ExpectedResult.Add(TEXT("ShooterGame"), EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("FortniteGame"), EUnitTestVerification::VerifiedFixed);

	NetDriverLog = TEXT("LogWebSocketNetworking: GameNetDriver WebSocketNetDriver");
}

void UWebSocketClient::InitializeEnvironmentSettings()
{
	Super::InitializeEnvironmentSettings();

	BaseServerParameters = UnitEnv->GetDefaultServerParameters(TEXT("LogWebSocketNetworking log")) +
							TEXT(" -NetDriverOverrides=/Script/WebSocketNetworking.WebSocketNetDriver");
}

void UWebSocketClient::NotifyAlterMinClient(FMinClientParms& Parms)
{
	Super::NotifyAlterMinClient(Parms);

	Parms.NetDriverClass = TEXT("/Script/WebSocketNetworking.WebSocketNetDriver");
}
