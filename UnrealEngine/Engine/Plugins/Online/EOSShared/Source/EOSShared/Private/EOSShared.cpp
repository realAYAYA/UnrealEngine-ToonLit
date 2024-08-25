// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EOS_SDK

#include "EOSShared.h"

#include "String/ParseTokens.h"
#include "EOSSharedTypes.h"

#include "eos_auth_types.h"
#include "eos_friends_types.h"
#include "eos_presence_types.h"
#include "eos_rtc_types.h"
#include "eos_userinfo_types.h"

DEFINE_LOG_CATEGORY(LogEOSSDK);

FString LexToString(const EOS_EResult EosResult)
{
	return UTF8_TO_TCHAR(EOS_EResult_ToString(EosResult));
}

FString LexToString(const EOS_ProductUserId UserId)
{
	FString Result;

	char ProductIdString[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
	ProductIdString[0] = '\0';
	int32_t BufferSize = sizeof(ProductIdString);
	if (EOS_ProductUserId_IsValid(UserId) == EOS_TRUE &&
		EOS_ProductUserId_ToString(UserId, ProductIdString, &BufferSize) == EOS_EResult::EOS_Success)
	{
		Result = UTF8_TO_TCHAR(ProductIdString);
	}

	return Result;
}

void LexFromString(EOS_ProductUserId& UserId, const TCHAR* String)
{
	UserId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(String));
}

FString LexToString(const EOS_EpicAccountId AccountId)
{
	FString Result;

	char AccountIdString[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
	AccountIdString[0] = '\0';
	int32_t BufferSize = sizeof(AccountIdString);
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_TRUE &&
		EOS_EpicAccountId_ToString(AccountId, AccountIdString, &BufferSize) == EOS_EResult::EOS_Success)
	{
		Result = UTF8_TO_TCHAR(AccountIdString);
	}

	return Result;
}

void LexFromString(EOS_EpicAccountId& AccountId, const TCHAR* String)
{
	AccountId = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(String));
}

const TCHAR* LexToString(const EOS_EApplicationStatus ApplicationStatus)
{
	switch (ApplicationStatus)
	{
		case EOS_EApplicationStatus::EOS_AS_BackgroundConstrained:		return TEXT("BackgroundConstrained");
		case EOS_EApplicationStatus::EOS_AS_BackgroundUnconstrained:	return TEXT("BackgroundUnconstrained");
		case EOS_EApplicationStatus::EOS_AS_BackgroundSuspended:		return TEXT("BackgroundSuspended");
		case EOS_EApplicationStatus::EOS_AS_Foreground:					return TEXT("Foreground");
		default: checkNoEntry();										return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_EAuthTokenType AuthTokenType)
{
	switch (AuthTokenType)
	{
		case EOS_EAuthTokenType::EOS_ATT_Client:	return TEXT("Client");
		case EOS_EAuthTokenType::EOS_ATT_User:		return TEXT("User");
		default: checkNoEntry();					return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_EDesktopCrossplayStatus DesktopCrossplayStatus)
{
	switch (DesktopCrossplayStatus)
	{
		case EOS_EDesktopCrossplayStatus::EOS_DCS_OK:							return TEXT("OK");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_ApplicationNotBootstrapped:	return TEXT("ApplicationNotBootstrapped");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_ServiceNotInstalled:			return TEXT("ServiceNotInstalled");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_ServiceStartFailed:			return TEXT("ServiceStartFailed");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_ServiceNotRunning:			return TEXT("ServiceNotRunning");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_OverlayDisabled:				return TEXT("OverlayDisabled");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_OverlayNotInstalled:			return TEXT("OverlayNotInstalled");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_OverlayTrustCheckFailed:		return TEXT("OverlayTrustCheckFailed");
		case EOS_EDesktopCrossplayStatus::EOS_DCS_OverlayLoadFailed:			return TEXT("OverlayLoadFailed");
		default: checkNoEntry();												return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_EExternalAccountType ExternalAccountType)
{
	switch (ExternalAccountType)
	{
		case EOS_EExternalAccountType::EOS_EAT_EPIC:		return TEXT("Epic");
		case EOS_EExternalAccountType::EOS_EAT_STEAM:		return TEXT("Steam");
		case EOS_EExternalAccountType::EOS_EAT_PSN:			return TEXT("PSN");
		case EOS_EExternalAccountType::EOS_EAT_XBL:			return TEXT("XBL");
		case EOS_EExternalAccountType::EOS_EAT_DISCORD:		return TEXT("Discord");
		case EOS_EExternalAccountType::EOS_EAT_GOG:			return TEXT("GOG");
		case EOS_EExternalAccountType::EOS_EAT_NINTENDO:	return TEXT("Nintendo");
		case EOS_EExternalAccountType::EOS_EAT_UPLAY:		return TEXT("UPlay");
		case EOS_EExternalAccountType::EOS_EAT_OPENID:		return TEXT("OpenID");
		case EOS_EExternalAccountType::EOS_EAT_APPLE:		return TEXT("Apple");
		case EOS_EExternalAccountType::EOS_EAT_GOOGLE:		return TEXT("Google");
		case EOS_EExternalAccountType::EOS_EAT_OCULUS:		return TEXT("Oculus");
		case EOS_EExternalAccountType::EOS_EAT_ITCHIO:		return TEXT("ItchIO");
		case EOS_EExternalAccountType::EOS_EAT_AMAZON:		return TEXT("Amazon");
		default: checkNoEntry();							return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_EFriendsStatus FriendStatus)
{
	switch (FriendStatus)
	{
		default: checkNoEntry(); // Intentional fall through
		case EOS_EFriendsStatus::EOS_FS_NotFriends:		return TEXT("NotFriends");
		case EOS_EFriendsStatus::EOS_FS_InviteSent:		return TEXT("InviteSent");
		case EOS_EFriendsStatus::EOS_FS_InviteReceived: return TEXT("InviteReceived");
		case EOS_EFriendsStatus::EOS_FS_Friends:		return TEXT("Friends");
	}
}

const TCHAR* LexToString(const EOS_ELoginStatus LoginStatus)
{
	switch (LoginStatus)
	{
		case EOS_ELoginStatus::EOS_LS_NotLoggedIn:			return TEXT("NotLoggedIn");
		case EOS_ELoginStatus::EOS_LS_UsingLocalProfile:	return TEXT("UsingLocalProfile");
		case EOS_ELoginStatus::EOS_LS_LoggedIn:				return TEXT("LoggedIn");
		default: checkNoEntry();							return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_ENetworkStatus NetworkStatus)
{
	switch (NetworkStatus)
	{
		case EOS_ENetworkStatus::EOS_NS_Disabled:	return TEXT("Disabled");
		case EOS_ENetworkStatus::EOS_NS_Offline:	return TEXT("Offline");
		case EOS_ENetworkStatus::EOS_NS_Online:		return TEXT("Online");
		default: checkNoEntry();					return TEXT("Unknown");
	}
}

const TCHAR* LexToString(const EOS_Presence_EStatus PresenceStatus)
{
	switch (PresenceStatus)
	{
		case EOS_Presence_EStatus::EOS_PS_Offline:		return TEXT("Offline");
		case EOS_Presence_EStatus::EOS_PS_Online:		return TEXT("Online");
		case EOS_Presence_EStatus::EOS_PS_Away:			return TEXT("Away");
		case EOS_Presence_EStatus::EOS_PS_ExtendedAway:	return TEXT("ExtendedAway");
		case EOS_Presence_EStatus::EOS_PS_DoNotDisturb:	return TEXT("DoNotDisturb");
		default: checkNoEntry();						return TEXT("Unknown");
	}
}

bool LexFromString(EOS_EAuthScopeFlags& OutEnum, const FStringView& InString)
{
	OutEnum = EOS_EAuthScopeFlags::EOS_AS_NoFlags;
	bool bParsedOk = true;

	using namespace UE::String;
	const EParseTokensOptions ParseOptions = EParseTokensOptions::SkipEmpty | EParseTokensOptions::Trim;
	auto ParseFunc = [&OutEnum, &bParsedOk](FStringView Token)
	{
		if (Token == TEXT("BasicProfile"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
		}
		else if (Token == TEXT("FriendsList"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_FriendsList;
		}
		else if (Token == TEXT("Presence"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_Presence;
		}
		else if (Token == TEXT("FriendsManagement"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_FriendsManagement;
		}
		else if (Token == TEXT("Email"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_Email;
		}
		else if (Token == TEXT("Country"))
		{
			OutEnum |= EOS_EAuthScopeFlags::EOS_AS_Country;
		}
		else
		{
			checkNoEntry();
			bParsedOk = false;
		}
	};

	ParseTokens(InString, TCHAR('|'), (TFunctionRef<void(FStringView)>)ParseFunc, ParseOptions);

	return bParsedOk;
}

bool LexFromString(EOS_ELoginCredentialType& OutEnum, const TCHAR* InString)
{
	if (FCString::Stricmp(InString, TEXT("ExchangeCode")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
	}
	else if (FCString::Stricmp(InString, TEXT("PersistentAuth")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
	}
	else if (FCString::Stricmp(InString, TEXT("Password")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_Password;
	}
	else if (FCString::Stricmp(InString, TEXT("Developer")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_Developer;
	}
	else if (FCString::Stricmp(InString, TEXT("RefreshToken")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_RefreshToken;
	}
	else if (FCString::Stricmp(InString, TEXT("AccountPortal")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
	}
	else if (FCString::Stricmp(InString, TEXT("ExternalAuth")) == 0)
	{
		OutEnum = EOS_ELoginCredentialType::EOS_LCT_ExternalAuth;
	}
	else
	{
		return false;
	}
	return true;
}

FString GetBestDisplayNameStr(const EOS_UserInfo_BestDisplayName& BestDisplayName)
{
	return FString(UTF8_TO_TCHAR(BestDisplayName.Nickname ? BestDisplayName.Nickname : BestDisplayName.DisplayNameSanitized ? BestDisplayName.DisplayNameSanitized : BestDisplayName.DisplayName));
}

bool LexFromString(EOS_ERTCBackgroundMode& OutEnum, const TCHAR* InString)
{
	if (FCString::Stricmp(InString, TEXT("LeaveRooms")) == 0)
	{
		OutEnum = EOS_ERTCBackgroundMode::EOS_RTCBM_LeaveRooms;
	}
	else if (FCString::Stricmp(InString, TEXT("KeepRoomsAlive")) == 0)
	{
		OutEnum = EOS_ERTCBackgroundMode::EOS_RTCBM_KeepRoomsAlive;
	}
	else
	{
		checkNoEntry();
		return false;
	}

	return true;
}

//TODO: Add support for multiple flags set
bool LexFromString(EOS_UI_EInputStateButtonFlags& OutEnum, const TCHAR* InString)
{
	if (FCString::Stricmp(InString, TEXT("DPad_Left")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_DPad_Left;
	}
	else if (FCString::Stricmp(InString, TEXT("DPad_Right")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_DPad_Right;
	}
	else if (FCString::Stricmp(InString, TEXT("DPad_Down")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_DPad_Down;
	}
	else if (FCString::Stricmp(InString, TEXT("DPad_Up")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_DPad_Up;
	}
	else if (FCString::Stricmp(InString, TEXT("FaceButton_Left")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_FaceButton_Left;
	}
	else if (FCString::Stricmp(InString, TEXT("FaceButton_Right")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_FaceButton_Right;
	}
	else if (FCString::Stricmp(InString, TEXT("FaceButton_Bottom")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_FaceButton_Bottom;
	}
	else if (FCString::Stricmp(InString, TEXT("FaceButton_Top")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_FaceButton_Top;
	}
	else if (FCString::Stricmp(InString, TEXT("LeftShoulder")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_LeftShoulder;
	}
	else if (FCString::Stricmp(InString, TEXT("RightShoulder")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_RightShoulder;
	}
	else if (FCString::Stricmp(InString, TEXT("LeftTrigger")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_LeftTrigger;
	}
	else if (FCString::Stricmp(InString, TEXT("RightTrigger")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_RightTrigger;
	}
	else if (FCString::Stricmp(InString, TEXT("Special_Left")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_Special_Left;
	}
	else if (FCString::Stricmp(InString, TEXT("Special_Right")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_Special_Right;
	}
	else if (FCString::Stricmp(InString, TEXT("LeftThumbstick")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_LeftThumbstick;
	}
	else if (FCString::Stricmp(InString, TEXT("RightThumbstick")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_RightThumbstick;
	}
	else if (FCString::Stricmp(InString, TEXT("None")) == 0)
	{
		OutEnum = EOS_UI_EInputStateButtonFlags::EOS_UISBF_None;
	}
	else
	{
		checkNoEntry();
		return false;
	}

	return true;
}

bool LexFromString(EOS_EIntegratedPlatformManagementFlags& OutEnum, const TCHAR* InString)
{
	if (FCString::Stricmp(InString, TEXT("ApplicationManagedIdentityLogin")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_ApplicationManagedIdentityLogin;
	}
	else if (FCString::Stricmp(InString, TEXT("Disabled")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_Disabled;
	}
	else if (FCString::Stricmp(InString, TEXT("DisablePresenceMirroring")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_DisablePresenceMirroring;
	}
	else if (FCString::Stricmp(InString, TEXT("DisableSDKManagedSessions")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_DisableSDKManagedSessions;
	}
	else if (FCString::Stricmp(InString, TEXT("LibraryManagedByApplication")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_LibraryManagedByApplication;
	}
	else if (FCString::Stricmp(InString, TEXT("LibraryManagedBySDK")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_LibraryManagedBySDK;
	}
	else if (FCString::Stricmp(InString, TEXT("PreferEOSIdentity")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_PreferEOSIdentity;
	}
	else if (FCString::Stricmp(InString, TEXT("PreferIntegratedIdentity")) == 0)
	{
		OutEnum = EOS_EIntegratedPlatformManagementFlags::EOS_IPMF_PreferIntegratedIdentity;
	}
	else
	{
		checkNoEntry();
		return false;
	}

	return true;
}

EOS_OnlinePlatformType EOSOnlinePlatformTypeFromString(const FStringView& InString)
{
	if (InString == TEXT("Unknown"))
	{
		return EOS_OPT_Unknown;
	}
	else if (InString == TEXT("Epic"))
	{	
		return EOS_OPT_Epic;
	}
	else if (InString == TEXT("Steam"))
	{	
		return EOS_OPT_Steam;
	}
	else if (InString == TEXT("PSN"))
	{	
		return 1000; //EOS_OPT_PSN;
	}
	else if (InString == TEXT("Switch"))
	{	
		return 2000; //EOS_OPT_SWITCH;
	}
	else if (InString == TEXT("XBL"))
	{	
		return 3000; //EOS_OPT_XBL;
	}
	else
	{
		checkNoEntry();
		return EOS_OPT_Unknown;
	}
}

FString LexToString(const EOS_RTC_Option& Option)
{
	UE_EOS_CHECK_API_MISMATCH(EOS_RTC_OPTION_API_LATEST, 1);
	check(Option.ApiVersion == 1);
	return FString::Printf(TEXT("\"%hs\"=\"%hs\""), Option.Key, Option.Value);
}

#endif // WITH_EOS_SDK