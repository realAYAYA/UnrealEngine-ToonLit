// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/Auth.h"

namespace UE::Online {

namespace LoginCredentialsType
{
const FName Auto = TEXT("Auto");
const FName Password = TEXT("Password");
const FName ExchangeCode = TEXT("ExchangeCode");
const FName PersistentAuth = TEXT("PersistentAuth");
const FName Developer = TEXT("Developer");
const FName RefreshToken = TEXT("RefreshToken");
const FName AccountPortal = TEXT("AccountPortal");
const FName ExternalAuth = TEXT("ExternalAuth");
}

namespace ExternalLoginType
{
const FName Epic = TEXT("Epic");
const FName SteamAppTicket = TEXT("SteamAppTicket");
const FName PsnIdToken = TEXT("PsnIdToken");
const FName XblXstsToken = TEXT("XblXstsToken");
const FName DiscordAccessToken = TEXT("DiscordAccessToken");
const FName GogSessionTicket = TEXT("GogSessionTicket");
const FName NintendoIdToken = TEXT("NintendoIdToken");
const FName NintendoNsaIdToken = TEXT("NintendoNsaIdToken");
const FName UplayAccessToken = TEXT("UplayAccessToken");
const FName OpenIdAccessToken = TEXT("OpenIdAccessToken");
const FName DeviceIdAccessToken = TEXT("DeviceIdAccessToken");
const FName AppleIdToken = TEXT("AppleIdToken");
const FName GoogleIdToken = TEXT("GoogleIdToken");
const FName OculusUserIdNonce = TEXT("OculusUserIdNonce");
const FName ItchioJwt = TEXT("ItchioJwt");
const FName ItchioKey = TEXT("ItchioKey");
const FName EpicIdToken = TEXT("EpicIdToken");
const FName AmazonAccessToken = TEXT("AmazonAccessToken");
}

namespace ExternalServerAuthTicketType
{
const FName PsnAuthCode = TEXT("PsnAuthCode");
const FName XblXstsToken = TEXT("XblXstsToken");
}

namespace AccountAttributeData
{
const FSchemaAttributeId DisplayName = TEXT("DisplayName");
}

const TCHAR* LexToString(ELoginStatus Status)
{
	switch (Status)
	{
	case ELoginStatus::UsingLocalProfile:	return TEXT("UsingLocalProfile");
	case ELoginStatus::LoggedIn:			return TEXT("LoggedIn");
	case ELoginStatus::LoggedInReducedFunctionality:	return TEXT("LoggedInReducedFunctionality");
	default:								checkNoEntry(); // Intentional fallthrough
	case ELoginStatus::NotLoggedIn:			return TEXT("NotLoggedIn");
	}
}

void LexFromString(ELoginStatus& OutStatus, const TCHAR* InStr)
{
	if (FCString::Stricmp(InStr, TEXT("LoggedIn")) == 0)
	{
		OutStatus = ELoginStatus::LoggedIn;
	}
	else if (FCString::Stricmp(InStr, TEXT("UsingLocalProfile")) == 0)
	{
		OutStatus = ELoginStatus::UsingLocalProfile;
	}
	else if (FCString::Stricmp(InStr, TEXT("LoggedInReducedFunctionality")) == 0)
	{
		OutStatus = ELoginStatus::LoggedInReducedFunctionality;
	}
	else if (FCString::Stricmp(InStr, TEXT("NotLoggedIn")) == 0)
	{
		OutStatus = ELoginStatus::NotLoggedIn;
	}
	else
	{
		checkNoEntry();
		OutStatus = ELoginStatus::NotLoggedIn;
	}
}

const TCHAR* LexToString(ERemoteAuthTicketAudience Audience)
{
	switch (Audience)
	{
	case ERemoteAuthTicketAudience::DedicatedServer:	return TEXT("DedicatedServer");
	default:											checkNoEntry(); // Intentional fallthrough
	case ERemoteAuthTicketAudience::Peer:				return TEXT("Peer");
	}
}

void LexFromString(ERemoteAuthTicketAudience& OutAudience, const TCHAR* InStr)
{
	if (FCString::Stricmp(InStr, TEXT("Peer")) == 0)
	{
		OutAudience = ERemoteAuthTicketAudience::Peer;
	}
	else if (FCString::Stricmp(InStr, TEXT("DedicatedServer")) == 0)
	{
		OutAudience = ERemoteAuthTicketAudience::DedicatedServer;
	}
	else
	{
		checkNoEntry();
		OutAudience = ERemoteAuthTicketAudience::Peer;
	}
}

const TCHAR* LexToString(EExternalAuthTokenMethod Method)
{
	switch (Method)
	{
	case EExternalAuthTokenMethod::Primary:		return TEXT("Primary");
	default:									checkNoEntry(); // Intentional fallthrough
	case EExternalAuthTokenMethod::Secondary:	return TEXT("Secondary");
	}
}

void LexFromString(EExternalAuthTokenMethod& OutMethod, const TCHAR* InStr)
{
	if (FCString::Stricmp(InStr, TEXT("Primary")) == 0)
	{
		OutMethod = EExternalAuthTokenMethod::Primary;
	}
	else if (FCString::Stricmp(InStr, TEXT("Secondary")) == 0)
	{
		OutMethod = EExternalAuthTokenMethod::Secondary;
	}
	else
	{
		checkNoEntry();
		OutMethod = EExternalAuthTokenMethod::Primary;
	}
}

/* UE::Online */ }
