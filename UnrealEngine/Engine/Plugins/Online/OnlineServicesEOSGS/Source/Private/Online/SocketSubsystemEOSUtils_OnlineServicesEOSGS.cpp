// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_ENGINE
#include "SocketSubsystemEOSUtils_OnlineServicesEOSGS.h"

#include "Online/AuthEOSGS.h"
#include "Online/LobbiesEOSGS.h"
#include "Online/OnlineIdEOSGS.h"
#include "Online/OnlineServicesEOSGS.h"
#include "Online/OnlineServicesEOSGSTypes.h"

namespace UE::Online {

FSocketSubsystemEOSUtils_OnlineServicesEOS::FSocketSubsystemEOSUtils_OnlineServicesEOS(FOnlineServicesEOSGS& InServicesEOSGS)
	: ServicesEOSGS(InServicesEOSGS)
{
}

FSocketSubsystemEOSUtils_OnlineServicesEOS::~FSocketSubsystemEOSUtils_OnlineServicesEOS()
{
}

EOS_ProductUserId FSocketSubsystemEOSUtils_OnlineServicesEOS::GetLocalUserId()
{
	EOS_ProductUserId Result = nullptr;

	IAuthPtr AuthEOS = ServicesEOSGS.GetAuthInterface();
	check(AuthEOS);

	FAuthGetLocalOnlineUserByPlatformUserId::Params AuthParams;
	AuthParams.PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> AuthResult = AuthEOS->GetLocalOnlineUserByPlatformUserId(MoveTemp(AuthParams));
	if (AuthResult.IsOk())
	{
		UE::Online::FAuthGetLocalOnlineUserByPlatformUserId::Result OkValue = AuthResult.GetOkValue();

		Result = GetProductUserIdChecked(OkValue.AccountInfo->AccountId);
	}
	else
	{
		UE_LOG(LogOnlineServices, Verbose, TEXT("[FSocketSubsystemEOSUtils_OnlineServicesEOS::GetLocalUserId] Unable to get account for platform user id [%s]. Error=[%s]"), *ToLogString(AuthParams.PlatformUserId), *AuthResult.GetErrorValue().GetLogString(true));
	}

	return Result;
}

FString FSocketSubsystemEOSUtils_OnlineServicesEOS::GetSessionId()
{
	FString Result;

	IAuthPtr AuthEOS = ServicesEOSGS.GetAuthInterface();
	check(AuthEOS);

	FAuthGetLocalOnlineUserByPlatformUserId::Params AuthParams;
	AuthParams.PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> AuthResult = AuthEOS->GetLocalOnlineUserByPlatformUserId(MoveTemp(AuthParams));
	if (AuthResult.IsOk())
	{
		FAuthGetLocalOnlineUserByPlatformUserId::Result* AuthOkValue = AuthResult.TryGetOkValue();

		ILobbiesPtr LobbiesEOS = ServicesEOSGS.GetLobbiesInterface();
		check(LobbiesEOS);

		FGetJoinedLobbies::Params LobbiesParams;
		LobbiesParams.LocalAccountId = AuthOkValue->AccountInfo->AccountId;
		TOnlineResult<FGetJoinedLobbies> LobbiesResult = LobbiesEOS->GetJoinedLobbies(MoveTemp(LobbiesParams));
		if (LobbiesResult.IsOk())
		{
			FGetJoinedLobbies::Result* LobbiesOkValue = LobbiesResult.TryGetOkValue();

			// TODO: Pending support in Lobbies interface
			/*for (TSharedRef<FLobby> Lobby : LobbiesOkValue->Lobbies)
			{
					if (Lobby->SessionName == NAME_GameSession)
					Result = LobbiesEOS->GetLobbyIdString(Lobby);
			}*/
		}
		else
		{
			UE_LOG(LogOnlineServices, Verbose, TEXT("[FSocketSubsystemEOSUtils_OnlineServicesEOS::GetSessionId] Unable to get joined lobbies for local user id [%s]. Error=[%s]"), *ToLogString(LobbiesParams.LocalAccountId), *AuthResult.GetErrorValue().GetLogString(true));
		}
	}
	else
	{
		UE_LOG(LogOnlineServices, Verbose, TEXT("[FSocketSubsystemEOSUtils_OnlineServicesEOS::GetSessionId] Unable to get account for platform user id [%s]. Error=[%s]"), *ToLogString(AuthParams.PlatformUserId), *AuthResult.GetErrorValue().GetLogString(true));
	}

	return Result;
}

FName FSocketSubsystemEOSUtils_OnlineServicesEOS::GetSubsystemInstanceName()
{
	return ServicesEOSGS.GetInstanceName();
}

/* UE::Online */}

#endif // WITH_ENGINE
