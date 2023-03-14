// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnitTests/Engine/SteamClient.h"

#include "Engine/Engine.h"

#include "MinimalClient.h"
#include "UnitTestEnvironment.h"

// @todo #JohnB: Perhaps add detection for whether or not the Steam Client is running.

/**
 * USteamClient
 */

USteamClient::USteamClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bPendingConnect(false)
{
	UnitTestName = TEXT("SteamClient");

	UnitTestDate = FDateTime(2017, 10, 5);

	UnitTestBugTrackIDs.Empty();

	ExpectedResult.Add(TEXT("ShooterGame"), EUnitTestVerification::VerifiedFixed);

	// @todo #JohnB: Not supported yet
	//ExpectedResult.Add(TEXT("FortniteGame"), EUnitTestVerification::VerifiedNotFixed);

	NetDriverLog = TEXT("LogNet: GameNetDriver SteamNetDriver");
}

bool USteamClient::ValidateUnitTestSettings(bool bCDOCheck)
{
	bool bSuccess = Super::ValidateUnitTestSettings(bCDOCheck);

	if (bSuccess && FParse::Param(FCommandLine::Get(), TEXT("-NoSteam")))
	{
		UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("Unit test can't run with Steam disabled."));

		bSuccess = false;
	}

	return bSuccess;
}

void USteamClient::InitializeEnvironmentSettings()
{
	Super::InitializeEnvironmentSettings();

	BaseServerParameters += TEXT(" -NetDriverOverrides=OnlineSubsystemSteam.SteamNetDriver");

	// The base unit test code tries to disable Steam - strip that out
	BaseServerParameters = BaseServerParameters.Replace(TEXT("-NoSteam"), TEXT(""));

	// Need to modify the local environment too, to enable Steam and disable attempts to init the Steam server API, on the client
	GEngine->Exec(nullptr, TEXT("OSS.SteamUnitTest 1"));
	GEngine->Exec(nullptr, TEXT("OSS.SteamInitServerOnClient 0"));
}

FString USteamClient::ConstructServerParameters()
{
	FString ReturnVal = Super::ConstructServerParameters();

	// The base ClientUnitTest code deliberately tries to disable Steam, using bIsLanMatch - strip that out, here
	ReturnVal = ReturnVal.Replace(TEXT("?bIsLanMatch"), TEXT(""));

	return ReturnVal;
}

bool USteamClient::ConnectMinimalClient(const TCHAR* InNetID/*=nullptr*/)
{
	bool bReturnVal = true;

	if (ServerSteamAddress.Len() > 0)
	{
		bReturnVal = Super::ConnectMinimalClient(InNetID);
	}
	else
	{
		bPendingConnect = true;
	}

	return bReturnVal;
}

void USteamClient::NotifyAlterMinClient(FMinClientParms& Parms)
{
	Super::NotifyAlterMinClient(Parms);

	Parms.NetDriverClass = TEXT("OnlineSubsystemSteam.SteamNetDriver");
	Parms.ServerAddress = FString(TEXT("Steam.")) + ServerSteamAddress + Parms.ServerAddress.Mid(Parms.ServerAddress.Find(TEXT(":")));
}

void USteamClient::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	Super::NotifyProcessLog(InProcess, InLogLines);

	const TCHAR* ServerAddrLog = TEXT("LogOnline: Verbose: STEAM: Master Server Data (P2PADDR, ");

	if (InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		for (const FString& CurLine : InLogLines)
		{
			int32 AddrIdx = CurLine.Find(ServerAddrLog);

			if (AddrIdx != INDEX_NONE)
			{
				ServerSteamAddress = FString::Printf(TEXT("%llu"),
														FCString::Atoi64(*CurLine.Mid(AddrIdx + FCString::Strlen(ServerAddrLog))));

				UE_LOG(LogUnitTest, Log, TEXT("Parsed server Steam address: %s"), *ServerSteamAddress);

				if (bPendingConnect)
				{
					bPendingConnect = false;

					ResetTimeout(TEXT("ServerSteamAddress"));
					ConnectMinimalClient();
				}

				break;
			}
		}
	}
}
