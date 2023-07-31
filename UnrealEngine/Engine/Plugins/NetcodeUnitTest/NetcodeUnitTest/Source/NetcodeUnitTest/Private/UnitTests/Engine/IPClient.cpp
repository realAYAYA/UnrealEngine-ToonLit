// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnitTests/Engine/IPClient.h"

#include "UnitTestEnvironment.h"
#include "MinimalClient.h"
#include "NUTActor.h"


/**
 * UIPClient
 */

UIPClient::UIPClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NetDriverLog(TEXT("LogNet: GameNetDriver IpNetDriver"))
	, ConnectSuccessMsg(TEXT("Unit test connect success."))
	, bDetectedNetDriver(false)
{
	UnitTestName = TEXT("IPClient");
	UnitTestType = TEXT("EngineTest");

	UnitTestDate = FDateTime(2017, 7, 19);

	ExpectedResult.Add(TEXT("ShooterGame"), EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("FortniteGame"), EUnitTestVerification::VerifiedFixed);

	UnitTestTimeout = 60;

	SetFlags<EUnitTestFlags::LaunchServer | EUnitTestFlags::AcceptPlayerController | EUnitTestFlags::RequireNUTActor,
				EMinClientFlags::AcceptActors | EMinClientFlags::NotifyNetActors>();
}

void UIPClient::InitializeEnvironmentSettings()
{
	BaseServerURL = UnitEnv->GetDefaultMap(UnitTestFlags);
	BaseServerParameters = UnitEnv->GetDefaultServerParameters();

	UnitEnv->InitializeUnitTasks();
}

void UIPClient::ExecuteClientUnitTest()
{
	if (bDetectedNetDriver)
	{
		UNIT_LOG(ELogType::StatusImportant, TEXT("Unit test ready - sending connection test log."));

		// Very basic test, just use a control channel message to trigger a log, to verify connect success (at a basic net driver level)
		SendNUTControl(ENUTControlCommand::Command_NoResult, ConnectSuccessMsg);
	}
	else
	{
		UNIT_LOG(ELogType::StatusImportant, TEXT("Failed to detect correct net driver."));

		VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
	}
}

void UIPClient::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	Super::NotifyProcessLog(InProcess, InLogLines);

	if (InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		for (auto CurLine : InLogLines)
		{
			if (!bDetectedNetDriver && CurLine.Contains(NetDriverLog))
			{
				bDetectedNetDriver = true;
			}

			if (CurLine.Contains(ConnectSuccessMsg))
			{
				VerificationState = EUnitTestVerification::VerifiedFixed;
				break;
			}
		}
	}
}
