// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserManagerEOS.h"

#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceRedirector.h"

#include "CoreMinimal.h"
#include "EOSSettings.h"
#include "IEOSSDKManager.h"
#include "IPAddress.h"
#include "OnlineError.h"
#include "OnlineSubsystemEOS.h"
#include "OnlineSubsystemEOSPrivate.h"
#include "OnlineSubsystemNames.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "SocketSubsystem.h"
#include "Serialization/JsonSerializable.h"
#include "Serialization/JsonSerializerMacros.h"

#include COMPILED_PLATFORM_HEADER(EOSHelpers.h)

#if WITH_EOS_SDK

#include "eos_auth.h"
#include "eos_userinfo.h"
#include "eos_friends.h"
#include "eos_presence.h"
#include "eos_ui.h"

static inline EInviteStatus::Type ToEInviteStatus(EOS_EFriendsStatus InStatus)
{
	switch (InStatus)
	{
		case EOS_EFriendsStatus::EOS_FS_InviteSent:
		{
			return EInviteStatus::PendingOutbound;
		}
		case EOS_EFriendsStatus::EOS_FS_InviteReceived:
		{
			return EInviteStatus::PendingInbound;
		}
		case EOS_EFriendsStatus::EOS_FS_Friends:
		{
			return EInviteStatus::Accepted;
		}
	}
	return EInviteStatus::Unknown;
}

static inline EOnlinePresenceState::Type ToEOnlinePresenceState(EOS_Presence_EStatus InStatus)
{
	switch (InStatus)
	{
		case EOS_Presence_EStatus::EOS_PS_Online:
		{
			return EOnlinePresenceState::Online;
		}
		case EOS_Presence_EStatus::EOS_PS_Away:
		{
			return EOnlinePresenceState::Away;
		}
		case EOS_Presence_EStatus::EOS_PS_ExtendedAway:
		{
			return EOnlinePresenceState::ExtendedAway;
		}
		case EOS_Presence_EStatus::EOS_PS_DoNotDisturb:
		{
			return EOnlinePresenceState::DoNotDisturb;
		}
	}
	return EOnlinePresenceState::Offline;
}

static inline EOS_Presence_EStatus ToEOS_Presence_EStatus(EOnlinePresenceState::Type InStatus)
{
	switch (InStatus)
	{
		case EOnlinePresenceState::Online:
		{
			return EOS_Presence_EStatus::EOS_PS_Online;
		}
		case EOnlinePresenceState::Away:
		{
			return EOS_Presence_EStatus::EOS_PS_Away;
		}
		case EOnlinePresenceState::ExtendedAway:
		{
			return EOS_Presence_EStatus::EOS_PS_ExtendedAway;
		}
		case EOnlinePresenceState::DoNotDisturb:
		{
			return EOS_Presence_EStatus::EOS_PS_DoNotDisturb;
		}
	}
	return EOS_Presence_EStatus::EOS_PS_Offline;
}

static inline EOS_EExternalCredentialType ToEOS_EExternalCredentialType(FName OSSName, const FOnlineAccountCredentials& AccountCredentials)
{
#if PLATFORM_DESKTOP
	if (OSSName == STEAM_SUBSYSTEM)
	{
		FEOSSettings Settings = UEOSSettings::GetSettings();
		if (Settings.SteamTokenType == TEXT("App"))
		{
			return EOS_EExternalCredentialType::EOS_ECT_STEAM_APP_TICKET;
		}
		// Session, WebApi, and WebApi:remoteserviceidentity are all "Session" tickets.
		return EOS_EExternalCredentialType::EOS_ECT_STEAM_SESSION_TICKET;
	}
#endif
	if (OSSName == PS4_SUBSYSTEM || USE_PSN_ID_TOKEN)
	{
		return EOS_EExternalCredentialType::EOS_ECT_PSN_ID_TOKEN;
	}
	else if (USE_XBL_XSTS_TOKEN)
	{
		return EOS_EExternalCredentialType::EOS_ECT_XBL_XSTS_TOKEN;
	}
	else if (OSSName == SWITCH_SUBSYSTEM)
	{
		if (AccountCredentials.Type == TEXT("NintendoAccount"))
		{
			return EOS_EExternalCredentialType::EOS_ECT_NINTENDO_ID_TOKEN;
		}
		else
		{
			return EOS_EExternalCredentialType::EOS_ECT_NINTENDO_NSA_ID_TOKEN;
		}
	}
	else if (OSSName == APPLE_SUBSYSTEM)
	{
		return EOS_EExternalCredentialType::EOS_ECT_APPLE_ID_TOKEN;
	}
	// Unknown means OpenID
	return EOS_EExternalCredentialType::EOS_ECT_OPENID_ACCESS_TOKEN;
}

bool ToEOS_ELoginCredentialType(const FString& InTypeStr, EOS_ELoginCredentialType& OutType)
{
	if (InTypeStr == TEXT("exchangecode"))
	{
		// This is how the Epic launcher will pass credentials to you
		OutType = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
	}
	else if (InTypeStr == TEXT("developer"))
	{
		// This is auth via the EOS auth tool
		OutType = EOS_ELoginCredentialType::EOS_LCT_Developer;
	}
	else if (InTypeStr == TEXT("password"))
	{
		// This is using a direct username / password. Restricted and not generally available.
		OutType = EOS_ELoginCredentialType::EOS_LCT_Password;
	}
	else if (InTypeStr == TEXT("accountportal"))
	{
		// This is auth via the EOS Account Portal
		OutType = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
	}
	else if (InTypeStr == TEXT("persistentauth"))
	{
		// Use locally stored token managed by EOSSDK keyring to attempt login.
		OutType = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
	}
	else if (InTypeStr == TEXT("externalauth"))
	{
		// Use external auth token to attempt login.
		OutType = EOS_ELoginCredentialType::EOS_LCT_ExternalAuth;
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Incorrect auth parameters (Type=%s)"), *InTypeStr);

		return false;
	}

	return true;
}

IOnlineSubsystem* GetPlatformOSS()
{
	IOnlineSubsystem* PlatformOSS = IOnlineSubsystem::GetByPlatform();
	if (PlatformOSS == nullptr)
#if !PLATFORM_DESKTOP
	{
		UE_LOG_ONLINE(Error, TEXT("GetPlatformOSS() failed due to no platform OSS being configured"));
	}
#else
	{
		// Attempt to load Steam before treating it as an error
		PlatformOSS = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	}
#endif
	return PlatformOSS;
}

namespace {

EOS_EAuthScopeFlags GetAuthScopeFlags()
{
	const FEOSSettings& Settings = UEOSSettings::GetSettings();

	EOS_EAuthScopeFlags ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_NoFlags;
	for (const FString& FlagsStr : Settings.AuthScopeFlags)
	{
		EOS_EAuthScopeFlags Flags;
		if (LexFromString(Flags, FlagsStr))
		{
			ScopeFlags |= Flags;
		}
	}
	
	return ScopeFlags;
}

} // namespace

/** Delegates that are used for internal calls and are meant to be ignored */
IOnlinePresence::FOnPresenceTaskCompleteDelegate IgnoredPresenceDelegate;

typedef TEOSGlobalCallback<EOS_UI_OnDisplaySettingsUpdatedCallback, EOS_UI_OnDisplaySettingsUpdatedCallbackInfo, FUserManagerEOS> FOnDisplaySettingsUpdatedCallback;

FUserManagerEOS::FUserManagerEOS(FOnlineSubsystemEOS* InSubsystem)
	: TSharedFromThis<FUserManagerEOS, ESPMode::ThreadSafe>()
	, EOSSubsystem(InSubsystem)
	, DefaultLocalUser(INVALID_LOCAL_USER)
	, LoginNotificationId(0)
	, LoginNotificationCallback(nullptr)
	, FriendsNotificationId(0)
	, FriendsNotificationCallback(nullptr)
	, PresenceNotificationId(0)
	, PresenceNotificationCallback(nullptr)
	, DisplaySettingsUpdatedId(0)
	, DisplaySettingsUpdatedCallback(nullptr)
{
}

void FUserManagerEOS::Init()
{
	// This delegate would cause a crash when running a dedicated server
	if (!IsRunningDedicatedServer())
	{
		// Adding subscription to external ui display change event
		EOS_UI_AddNotifyDisplaySettingsUpdatedOptions Options = {};
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_UI_ADDNOTIFYDISPLAYSETTINGSUPDATED_API_LATEST, 1);

		FOnDisplaySettingsUpdatedCallback* CallbackObj = new FOnDisplaySettingsUpdatedCallback(AsWeak());
		DisplaySettingsUpdatedCallback = CallbackObj;
		CallbackObj->CallbackLambda = [this](const EOS_UI_OnDisplaySettingsUpdatedCallbackInfo* Data)
		{
			TriggerOnExternalUIChangeDelegates((bool)Data->bIsVisible);
		};

		DisplaySettingsUpdatedId = EOS_UI_AddNotifyDisplaySettingsUpdated(EOSSubsystem->UIHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}
}

void FUserManagerEOS::Shutdown()
{
	// This delegate would cause a crash when running a dedicated server
	if (DisplaySettingsUpdatedId != EOS_INVALID_NOTIFICATIONID)
	{
		// Removing subscription to external ui display change event
		EOS_UI_RemoveNotifyDisplaySettingsUpdated(EOSSubsystem->UIHandle, DisplaySettingsUpdatedId);

		if (DisplaySettingsUpdatedCallback)
		{
			delete DisplaySettingsUpdatedCallback;
		}
	}
}

FUserManagerEOS::~FUserManagerEOS()
{
	Shutdown();
}

void FUserManagerEOS::LoginStatusChanged(const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
{
	if (Data->CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn)
	{
		if (IsLocalUser(Data->LocalUserId))
		{
			const int32 LocalUserNum = GetLocalUserNumFromEpicAccountId(Data->LocalUserId);
			FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);
			const FUniqueNetIdEOSPtr& UserNetId = LocalUser.UniqueNetId;
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserNetId);

			// Remove the per user connect login notification
			if (LocalUser.ConnectLoginNotification.IsValid())
			{
				EOS_Connect_RemoveNotifyAuthExpiration(EOSSubsystem->ConnectHandle, LocalUser.ConnectLoginNotification->NotificationId);
				LocalUser.ConnectLoginNotification.Reset();
			}

			// Need to remove the local user
			RemoveLocalUser(LocalUserNum);

			// Clean up user based notifications if we have no logged in users
			if (LocalUsers.Num() == 0)
			{
				if (LoginNotificationId > 0)
				{
					// Remove the callback
					EOS_Auth_RemoveNotifyLoginStatusChanged(EOSSubsystem->AuthHandle, LoginNotificationId);
					delete LoginNotificationCallback;
					LoginNotificationCallback = nullptr;
					LoginNotificationId = 0;
				}
				if (FriendsNotificationId > 0)
				{
					EOS_Friends_RemoveNotifyFriendsUpdate(EOSSubsystem->FriendsHandle, FriendsNotificationId);
					delete FriendsNotificationCallback;
					FriendsNotificationCallback = nullptr;
					FriendsNotificationId = 0;
				}
				if (PresenceNotificationId > 0)
				{
					EOS_Presence_RemoveNotifyOnPresenceChanged(EOSSubsystem->PresenceHandle, PresenceNotificationId);
					delete PresenceNotificationCallback;
					PresenceNotificationCallback = nullptr;
					PresenceNotificationId = 0;
				}
			}
		}
	}
}

void FUserManagerEOS::GetPlatformAuthToken(int32 LocalUserNum, const FOnGetLinkedAccountAuthTokenCompleteDelegate& Delegate) const
{
	IOnlineSubsystem* PlatformOSS = GetPlatformOSS();
	if (PlatformOSS == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("ConnectLoginNoEAS(%d) failed due to no platform OSS"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FExternalAuthToken());
		return;
	}
	IOnlineIdentityPtr PlatformIdentity = PlatformOSS->GetIdentityInterface();
	if (!PlatformIdentity.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("ConnectLoginNoEAS(%d) failed due to no platform OSS identity interface"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FExternalAuthToken());
		return;
	}

	FString TokenType;
	if (PlatformOSS->GetSubsystemName() == STEAM_SUBSYSTEM)
	{
		FEOSSettings Settings = UEOSSettings::GetSettings();
		TokenType = Settings.SteamTokenType;
	}

	// Request the auth token from the platform
	PlatformIdentity->GetLinkedAccountAuthToken(LocalUserNum, TokenType, Delegate);
}

FString FUserManagerEOS::GetPlatformDisplayName(int32 LocalUserNum) const
{
	FString Result;

	IOnlineSubsystem* PlatformOSS = GetPlatformOSS();
	if (PlatformOSS == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("GetPlatformDisplayName(%d) failed due to no platform OSS"), LocalUserNum);
		return Result;
	}
	IOnlineIdentityPtr PlatformIdentity = PlatformOSS->GetIdentityInterface();
	if (!PlatformIdentity.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("GetPlatformDisplayName(%d) failed due to no platform OSS identity interface"), LocalUserNum);
		return Result;
	}

	Result = PlatformIdentity->GetPlayerNickname(LocalUserNum);

	return Result;
}

typedef TEOSCallback<EOS_Auth_OnLoginCallback, EOS_Auth_LoginCallbackInfo, FUserManagerEOS> FLoginCallback;
typedef TEOSCallback<EOS_Connect_OnLoginCallback, EOS_Connect_LoginCallbackInfo, FUserManagerEOS> FConnectLoginCallback;
typedef TEOSCallback<EOS_Auth_OnDeletePersistentAuthCallback, EOS_Auth_DeletePersistentAuthCallbackInfo, FUserManagerEOS> FDeletePersistentAuthCallback;

bool FUserManagerEOS::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FEOSSettings Settings = UEOSSettings::GetSettings();

	// Are we configured to run at all?
	if (!EOSSubsystem->bIsDefaultOSS && !EOSSubsystem->bIsPlatformOSS && !Settings.bUseEAS && !Settings.bUseEOSConnect)
	{
		UE_LOG_ONLINE(Warning, TEXT("Neither EAS nor EOS are configured to be used. Failed to login in user (%d)"), LocalUserNum);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), FString(TEXT("Not configured")));
		return true;
	}

	// We don't support offline logged in, so they are either logged in or not
	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE(Warning, TEXT("User (%d) already logged in."), LocalUserNum);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), FString(TEXT("Already logged in")));
		return true;
	}

	// See if we are configured to just use EOS and not EAS
	if (!EOSSubsystem->bIsDefaultOSS && !EOSSubsystem->bIsPlatformOSS && !Settings.bUseEAS && Settings.bUseEOSConnect)
	{
		// Call the EOS + Platform login path
		return ConnectLoginNoEAS(LocalUserNum, AccountCredentials);
	}

	// See if we are logging in using platform credentials to link to EAS
	if (!EOSSubsystem->bIsDefaultOSS && !EOSSubsystem->bIsPlatformOSS && Settings.bUseEAS)
	{
		if (Settings.bPreferPersistentAuth)
		{
			LoginViaPersistentAuth(LocalUserNum, AccountCredentials);
		}
		else
		{
			LoginViaExternalAuth(LocalUserNum, AccountCredentials);
		}

		return true;
	}

	CallEOSAuthLogin(LocalUserNum, AccountCredentials);

	return true;
}

struct FSavedEOSAuthToken : public FJsonSerializable
{
	FString Token;
	FDateTime ExpirationDate;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("token", Token);
		JSON_SERIALIZE_DATETIME_UNIX_TIMESTAMP_MILLISECONDS("expirationDate", ExpirationDate);
	END_JSON_SERIALIZER
};

#define EOS_EPIC_AUTH_TOKEN_FILENAME_SUFFIX TEXT("EpicAuthToken")

FString FUserManagerEOS::GetEOSAuthTokenFilename()
{
	return EOSSubsystem->ProductId + TEXT("_") + EOS_EPIC_AUTH_TOKEN_FILENAME_SUFFIX;
}

void FUserManagerEOS::LoginViaPersistentAuth(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FPlatformEOSHelpersPtr EOSHelpers = EOSSubsystem->GetEOSHelpers();

#if EOS_AUTH_TOKEN_SAVEGAME_STORAGE
	ISaveGameSystem* SaveGameSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();

	FString SavedEOSAuthTokenFilename = GetEOSAuthTokenFilename();

	if (SaveGameSystem->DoesSaveGameExist(*SavedEOSAuthTokenFilename, LocalUserNum))
	{
		TArray<uint8> SavedEOSAuthTokenData;

		if (SaveGameSystem->LoadGame(false, *SavedEOSAuthTokenFilename, LocalUserNum, SavedEOSAuthTokenData))
		{
			const FString SavedEOSAuthTokenStr = BytesToString(SavedEOSAuthTokenData.GetData(), SavedEOSAuthTokenData.Num());

			FSavedEOSAuthToken SavedEOSAuthToken;
			SavedEOSAuthToken.FromJson(SavedEOSAuthTokenStr);

			UE_LOG_ONLINE(Verbose, TEXT("Loaded Saved EOS Auth Token from File [%s]. Contents: [%s]"), *SavedEOSAuthTokenFilename, UE_BUILD_SHIPPING ? TEXT("<redacted>") : *SavedEOSAuthTokenStr);

			// If the token has expired, we'll process login via Account Portal below
			if (SavedEOSAuthToken.ExpirationDate > FDateTime::Now())
			{
				// TODO: We should renew it a configurable amount of time (day or week) before it expires, to avoid having it expire in the middle of a game session
#endif
				EOS_Auth_Credentials Credentials = {};
				Credentials.ApiVersion = 4;
				UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_CREDENTIALS_API_LATEST, 4);
				Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
#if EOS_AUTH_TOKEN_SAVEGAME_STORAGE
				auto AuthTokenUtf8 = StringCast<UTF8CHAR>(*SavedEOSAuthToken.Token);
				Credentials.Token = (const char*)AuthTokenUtf8.Get();
#endif

				EOS_Auth_LoginOptions PALoginOptions = { };
				PALoginOptions.ApiVersion = 3;
				UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LOGIN_API_LATEST, 3);
				PALoginOptions.ScopeFlags = GetAuthScopeFlags();
				PALoginOptions.Credentials = &Credentials;

				EOSHelpers->GetSystemAuthCredentialsOptions(Credentials.SystemAuthCredentialsOptions);

				FLoginCallback* PACallbackObj = new FLoginCallback(AsWeak());
				PACallbackObj->CallbackLambda = [this, LocalUserNum, AccountCredentials](const EOS_Auth_LoginCallbackInfo* Data)
				{
					OnEOSAuthLoginComplete(LocalUserNum, AccountCredentials, true, Data);
				};

				EOS_Auth_Login(EOSSubsystem->AuthHandle, &PALoginOptions, (void*)PACallbackObj, PACallbackObj->GetCallbackPtr());

				return;
#if EOS_AUTH_TOKEN_SAVEGAME_STORAGE
			}
			else
			{
				// The saved token is expired, we'll delete the saved token and obtain a new one
				if (!SaveGameSystem->DeleteGame(false, *SavedEOSAuthTokenFilename, LocalUserNum))
				{
					UE_LOG_ONLINE(Warning, TEXT("Unable to delete Saved EOS Auth Token [%s] for LocalUserNum [%d]"), *SavedEOSAuthTokenFilename, LocalUserNum);
				}
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Unable to load Saved EOS Auth Token [%s] for LocalUserNum [%d]"), *SavedEOSAuthTokenFilename, LocalUserNum);
		}
	}

	// If we don't have an auth token saved or it's expired, we'll get one via AccountPortal login first
	LoginViaAccountPortal(LocalUserNum, AccountCredentials);
#endif
}

// Chose arbitrarily since the SDK doesn't define it
#define EOS_MAX_TOKEN_SIZE 4096

FString ToHexString(const TArray<uint8>& InToken)
{
	char TokenAnsi[EOS_MAX_TOKEN_SIZE];
	uint32_t InOutBufferLength = EOS_MAX_TOKEN_SIZE;
	EOS_ByteArray_ToString(InToken.GetData(), InToken.Num(), TokenAnsi, &InOutBufferLength);

	return FString(TokenAnsi);
}

void FUserManagerEOS::LoginViaExternalAuth(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	GetPlatformAuthToken(LocalUserNum,
		FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda([this, WeakThis = AsWeak(), AccountCredentials = FOnlineAccountCredentials(AccountCredentials)](int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken) mutable
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				if (!bWasSuccessful || !AuthToken.IsValid())
				{
					UE_LOG_ONLINE(Warning, TEXT("Unable to Login() user (%d) due to an empty platform auth token"), LocalUserNum);
					TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), FString(TEXT("Missing platform auth token")));
					return;
				}

				AccountCredentials.Type = FString(TEXT("externalauth"));

				if (AuthToken.HasTokenData())
				{
					AccountCredentials.Token = ToHexString(AuthToken.TokenData);
				}
				else if (AuthToken.HasTokenString())
				{
					AccountCredentials.Token = AuthToken.TokenString;
				}
				else
				{
					UE_LOG_ONLINE(Error, TEXT("FAuthCredentials object cannot be constructed with invalid FExternalAuthToken parameter"));
					TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), FString(TEXT("Invalid platform auth token")));
					return;
				}

				CallEOSAuthLogin(LocalUserNum, AccountCredentials);
			}
		}));
}

FLocalUserEOS& FUserManagerEOS::GetLocalUserChecked(int32 LocalUserNum)
{
	check(LocalUsers.IsValidIndex(LocalUserNum));
	return LocalUsers[LocalUserNum];
}

void FUserManagerEOS::LoginViaAccountPortal(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	EOS_Auth_Credentials EOSCredentials = {};
	EOSCredentials.ApiVersion = 4;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_CREDENTIALS_API_LATEST, 4);
	EOSCredentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;

	EOS_Auth_LoginOptions LoginOptions = { };
	LoginOptions.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LOGIN_API_LATEST, 3);
	LoginOptions.ScopeFlags = GetAuthScopeFlags();
	LoginOptions.Credentials = &EOSCredentials;

	FPlatformEOSHelpersPtr EOSHelpers = EOSSubsystem->GetEOSHelpers();
	EOSHelpers->GetSystemAuthCredentialsOptions(EOSCredentials.SystemAuthCredentialsOptions);

	FLoginCallback* CallbackObj = new FLoginCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, LocalUserNum, AccountCredentials](const EOS_Auth_LoginCallbackInfo* Data) mutable
	{
		OnEOSAuthLoginComplete(LocalUserNum, AccountCredentials, true, Data);
	};

	EOS_Auth_Login(EOSSubsystem->AuthHandle, &LoginOptions, (void*)CallbackObj, CallbackObj->GetCallbackPtr());
}

void FUserManagerEOS::CallEOSAuthLogin(int32 LocalUserNum, const FOnlineAccountCredentials& Credentials)
{
	// First we construct the EOS Credentials object
	EOS_Auth_Credentials EOSCredentials = {};

	if (!ToEOS_ELoginCredentialType(Credentials.Type, EOSCredentials.Type))
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to Login() user (%d) due to missing auth parameters"), LocalUserNum);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), FString(TEXT("Missing auth parameters")));
		return;
	}

	EOSCredentials.ApiVersion = 4;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_CREDENTIALS_API_LATEST, 4);

	auto IdConverter = StringCast<UTF8CHAR>(*Credentials.Id);
	EOSCredentials.Id = IdConverter.Length() ? (const char*)IdConverter.Get() : nullptr;
	auto TokenConverter = StringCast<UTF8CHAR>(*Credentials.Token);
	EOSCredentials.Token = TokenConverter.Length() ? (const char*)TokenConverter.Get() : nullptr;

	IOnlineSubsystem* PlatformOSS = GetPlatformOSS();
	EOSCredentials.ExternalType = ToEOS_EExternalCredentialType(PlatformOSS ? PlatformOSS->GetSubsystemName() : EOSSubsystem->GetSubsystemName(), Credentials);

	// We start preparing the Login call
	EOS_Auth_LoginOptions LoginOptions = { };
	LoginOptions.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LOGIN_API_LATEST, 3);
	LoginOptions.ScopeFlags = GetAuthScopeFlags();
	LoginOptions.Credentials = &EOSCredentials;

	FPlatformEOSHelpersPtr EOSHelpers = EOSSubsystem->GetEOSHelpers();
	EOSHelpers->GetSystemAuthCredentialsOptions(EOSCredentials.SystemAuthCredentialsOptions);

	// Store selection of persistent auth.
	// The persistent auth token is handled by the EOSSDK. On a login failure the persistent token may need to be deleted if it is invalid.
	const bool bIsPersistentLogin = EOSCredentials.Type == EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

	FLoginCallback* CallbackObj = new FLoginCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, LocalUserNum, Credentials, bIsPersistentLogin](const EOS_Auth_LoginCallbackInfo* Data)
	{
		OnEOSAuthLoginComplete(LocalUserNum, Credentials, bIsPersistentLogin, Data);
	};

	// Perform the auth call
	EOS_Auth_Login(EOSSubsystem->AuthHandle, &LoginOptions, (void*)CallbackObj, CallbackObj->GetCallbackPtr());
}

void FUserManagerEOS::CopyAndSaveEpicAuthToken(int32 LocalUserNum, const EOS_EpicAccountId& EpicAccountId)
{
	EOS_Auth_Token* AuthToken = nullptr;
	EOS_Auth_CopyUserAuthTokenOptions CopyOptions = { };
	CopyOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

	EOS_EResult CopyResult = EOS_Auth_CopyUserAuthToken(EOSSubsystem->AuthHandle, &CopyOptions, EpicAccountId, &AuthToken);
	if (CopyResult == EOS_EResult::EOS_Success)
	{
		FSavedEOSAuthToken SavedEOSAuthToken;
		SavedEOSAuthToken.Token = StringCast<TCHAR>(AuthToken->RefreshToken).Get();
		const FString RefreshExpiresAtTCHARPtr = StringCast<TCHAR>(AuthToken->RefreshExpiresAt).Get();
		if (FDateTime::ParseIso8601(*RefreshExpiresAtTCHARPtr, SavedEOSAuthToken.ExpirationDate))
		{
			FString SavedEOSAuthTokenStr = SavedEOSAuthToken.ToJson(false);

			TArray<uint8> SavedEOSAuthTokenData;
			SavedEOSAuthTokenData.SetNumUninitialized(SavedEOSAuthTokenStr.Len());
			StringToBytes(SavedEOSAuthTokenStr, SavedEOSAuthTokenData.GetData(), SavedEOSAuthTokenStr.Len());

			ISaveGameSystem* SaveGameSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();

			FString SavedEOSAuthTokenFilename = GetEOSAuthTokenFilename();

			if (SaveGameSystem->SaveGame(false, *SavedEOSAuthTokenFilename, LocalUserNum, SavedEOSAuthTokenData))
			{
				UE_LOG_ONLINE(Verbose, TEXT("Saved EOS Auth Token to File [%s]. Contents: [%s]"), *SavedEOSAuthTokenFilename, UE_BUILD_SHIPPING ? TEXT("<redacted>") : *SavedEOSAuthTokenStr);
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Unable to save EOS Auth Token [%s] for LocalUserNum [%d]"), *SavedEOSAuthTokenFilename, LocalUserNum);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Unable to parse RefreshExpiresAt for FSavedEOSAuthToken. EOS Auth Token for user [%d] was not saved"), LocalUserNum);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("EOS_Auth_CopyUserAuthToken failed with result [%s]"), *LexToString(CopyResult));
	}
}

void FUserManagerEOS::OnEOSAuthLoginComplete(int32 LocalUserNum, const FOnlineAccountCredentials& Credentials, bool bIsPersistentLogin, const EOS_Auth_LoginCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		if (bIsPersistentLogin)
		{
#if EOS_AUTH_TOKEN_SAVEGAME_STORAGE
			CopyAndSaveEpicAuthToken(LocalUserNum, Data->LocalUserId);
#endif
			EOSSubsystem->GetEOSHelpers()->AddExternalAccountMapping(EOSSubsystem->GetEOSPlatformHandle(), Data->LocalUserId, LocalUserNum);
		}

		// Continue the login process by getting the product user id for EAS only
		ConnectLoginEAS(LocalUserNum, Data->LocalUserId, Credentials);
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
	{
		// Link the account
		LinkEAS(LocalUserNum, Data->ContinuanceToken, Credentials);
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidAuth && bIsPersistentLogin)
	{
		// We attempted a Persistent Auth login but there is no stored token, we'll attempt Account Portal login automatically

		LoginViaAccountPortal(LocalUserNum, Credentials);
	}
	else
	{
		auto TriggerLoginFailure = [this, LocalUserNum, LoginResultCode = Data->ResultCode]()
		{
			FString ErrorString = FString::Printf(TEXT("Login(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(LoginResultCode)));
			UE_LOG_ONLINE(Warning, TEXT("%s"), *ErrorString);
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
		};

		const bool bShouldRemoveCachedToken =
			Data->ResultCode == EOS_EResult::EOS_AccessDenied ||
			Data->ResultCode == EOS_EResult::EOS_Auth_InvalidToken;

		// Check for invalid persistent login credentials.
		if (bIsPersistentLogin && bShouldRemoveCachedToken)
		{
			FDeletePersistentAuthCallback* DeleteAuthCallbackObj = new FDeletePersistentAuthCallback(AsWeak());
			DeleteAuthCallbackObj->CallbackLambda = [this, LocalUserNum, TriggerLoginFailure](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
			{
				// Deleting the auth token is best effort.
				TriggerLoginFailure();
			};

			EOS_Auth_DeletePersistentAuthOptions DeletePersistentAuthOptions;
			DeletePersistentAuthOptions.ApiVersion = 2;
			UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST, 2);
			DeletePersistentAuthOptions.RefreshToken = nullptr;
			EOS_Auth_DeletePersistentAuth(EOSSubsystem->AuthHandle, &DeletePersistentAuthOptions, (void*)DeleteAuthCallbackObj, DeleteAuthCallbackObj->GetCallbackPtr());

#if EOS_AUTH_TOKEN_SAVEGAME_STORAGE
			// After deleting the token at API level, we'll also delete any saved tokens for this user
			ISaveGameSystem* SaveGameSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();

			FString SavedEOSAuthTokenFilename = GetEOSAuthTokenFilename();

			if (!SaveGameSystem->DeleteGame(false, *SavedEOSAuthTokenFilename, LocalUserNum))
			{
				UE_LOG_ONLINE(Warning, TEXT("Unable to delete Saved EOS Auth Token [%s] for LocalUserNum [%d]"), *SavedEOSAuthTokenFilename, LocalUserNum);
			}
#endif
		}
		else
		{
			TriggerLoginFailure();
		}
	}
}

struct FLinkAccountOptions :
	public EOS_Auth_LinkAccountOptions
{
	FLinkAccountOptions(EOS_ContinuanceToken Token)
		: EOS_Auth_LinkAccountOptions()
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LINKACCOUNT_API_LATEST, 1);
		ContinuanceToken = Token;
	}
};

typedef TEOSCallback<EOS_Auth_OnLinkAccountCallback, EOS_Auth_LinkAccountCallbackInfo, FUserManagerEOS> FLinkAccountCallback;

void FUserManagerEOS::LinkEAS(int32 LocalUserNum, EOS_ContinuanceToken Token, const FOnlineAccountCredentials& AccountCredentials)
{
	FLinkAccountOptions Options(Token);
	FLinkAccountCallback* CallbackObj = new FLinkAccountCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, LocalUserNum, AccountCredentials](const EOS_Auth_LinkAccountCallbackInfo* Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			// Continue the login process by getting the product user id
			ConnectLoginEAS(LocalUserNum, Data->LocalUserId, AccountCredentials);
		}
		else
		{
			FString ErrorString = FString::Printf(TEXT("Login(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			UE_LOG_ONLINE(Warning, TEXT("%s"), *ErrorString);
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
		}
	};
	EOS_Auth_LinkAccount(EOSSubsystem->AuthHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
}

struct FConnectCredentials :
	public EOS_Connect_Credentials
{
	FConnectCredentials(EOS_EExternalCredentialType InType, const FExternalAuthToken& AuthToken) :
		EOS_Connect_Credentials()
	{
		if (AuthToken.HasTokenData())
		{
			Init(InType, AuthToken.TokenData);
		}
		else if (AuthToken.HasTokenString())
		{
			Init(InType, AuthToken.TokenString);
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("FConnectCredentials object cannot be constructed with invalid FExternalAuthToken parameter"));
		}
	}

	void Init(EOS_EExternalCredentialType InType, const FString& InTokenString)
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_CREDENTIALS_API_LATEST, 1);
		Token = TokenAnsi;
		Type = InType;

		FCStringAnsi::Strncpy(TokenAnsi, TCHAR_TO_UTF8(*InTokenString), InTokenString.Len() + 1);
	}

	void Init(EOS_EExternalCredentialType InType, const TArray<uint8>& InToken)
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_CREDENTIALS_API_LATEST, 1);
		Token = TokenAnsi;
		Type = InType;

		uint32_t InOutBufferLength = EOS_MAX_TOKEN_SIZE;
		EOS_ByteArray_ToString(InToken.GetData(), InToken.Num(), TokenAnsi, &InOutBufferLength);
	}

	char TokenAnsi[EOS_MAX_TOKEN_SIZE];
};

bool FUserManagerEOS::ConnectLoginNoEAS(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	GetPlatformAuthToken(LocalUserNum,
		FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda([this, WeakThis = AsWeak(), AccountCredentials](int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken)
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				if (!bWasSuccessful || !AuthToken.IsValid())
				{
					const FString ErrorString = FString::Printf(TEXT("ConnectLoginNoEAS(%d) failed due to the platform OSS giving an empty auth token"), LocalUserNum);
					UE_LOG_ONLINE(Warning, TEXT("%s"), *ErrorString);
					TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
					return;
				}

				// Now login into our EOS account
				FConnectCredentials Credentials(ToEOS_EExternalCredentialType(GetPlatformOSS()->GetSubsystemName(), AccountCredentials), AuthToken);
				EOS_Connect_LoginOptions Options = { };
				Options.ApiVersion = 2;
				UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_LOGIN_API_LATEST, 2);
				Options.Credentials = &Credentials;

#if ADD_USER_LOGIN_INFO
				EOS_Connect_UserLoginInfo UserLoginInfo = {};
				UserLoginInfo.ApiVersion = 2;
				UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_USERLOGININFO_API_LATEST, 2);
				const FTCHARToUTF8 DisplayNameUtf8(*GetPlatformDisplayName(LocalUserNum));
				UserLoginInfo.DisplayName = DisplayNameUtf8.Get();
				UserLoginInfo.NsaIdToken = nullptr;

				Options.UserLoginInfo = &UserLoginInfo;
#endif

				FConnectLoginCallback* CallbackObj = new FConnectLoginCallback(AsWeak());
				CallbackObj->CallbackLambda = [this, LocalUserNum, AccountCredentials](const EOS_Connect_LoginCallbackInfo* Data)
				{
					if (Data->ResultCode == EOS_EResult::EOS_Success)
					{
						// We have an account mapping to the platform account, skip to final login
						FullLoginCallback(LocalUserNum, nullptr, Data->LocalUserId, AccountCredentials);
					}
					else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
					{
						// We need to create the platform account mapping for this user using the continuation token
						CreateConnectedLogin(LocalUserNum, nullptr, Data->ContinuanceToken, AccountCredentials);
					}
					else
					{
						const FString ErrorString = FString::Printf(TEXT("ConnectLoginNoEAS(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
						UE_LOG_ONLINE(Warning, TEXT("%s"), *ErrorString);
						TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
					}
				};
				EOS_Connect_Login(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
			}
		}));

	return true;
}

bool FUserManagerEOS::ConnectLoginEAS(int32 LocalUserNum, EOS_EpicAccountId AccountId, const FOnlineAccountCredentials& AccountCredentials)
{
#if ADD_USER_LOGIN_INFO
	GetPlatformAuthToken(LocalUserNum,
		FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda([this, WeakThis = AsWeak(), AccountId, AccountCredentials](int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken)
			{
				if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
				{
					if (!bWasSuccessful || !AuthToken.IsValid())
					{
						const FString ErrorString = FString::Printf(TEXT("ConnectLoginEAS(%d) failed due to the platform OSS giving an empty auth token"), LocalUserNum);
						UE_LOG_ONLINE(Warning, TEXT("%s"), *ErrorString);
						TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
						return;
					}
#endif

					EOS_Auth_Token* EOSAuthToken = nullptr;
					EOS_Auth_CopyUserAuthTokenOptions CopyOptions = { };
					CopyOptions.ApiVersion = 1;
					UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

					EOS_EResult CopyResult = EOS_Auth_CopyUserAuthToken(EOSSubsystem->AuthHandle, &CopyOptions, AccountId, &EOSAuthToken);
					if (CopyResult == EOS_EResult::EOS_Success)
					{
						EOS_Connect_Credentials Credentials = { };
						Credentials.ApiVersion = 1;
						UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_CREDENTIALS_API_LATEST, 1);
						Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;
						Credentials.Token = EOSAuthToken->AccessToken;

#if ADD_USER_LOGIN_INFO
						EOS_Connect_UserLoginInfo UserLoginInfo = {};
						UserLoginInfo.ApiVersion = 2;
						UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_USERLOGININFO_API_LATEST, 2);
						auto AuthTokenConverter = StringCast<UTF8CHAR>(*AuthToken.TokenString);
						UserLoginInfo.NsaIdToken = (const char*)AuthTokenConverter.Get();
#endif

						EOS_Connect_LoginOptions Options = { };
						Options.ApiVersion = 2;
						UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_LOGIN_API_LATEST, 2);
						Options.Credentials = &Credentials;
#if ADD_USER_LOGIN_INFO
						Options.UserLoginInfo = &UserLoginInfo;
#else
						Options.UserLoginInfo = nullptr;
#endif

						FConnectLoginCallback* CallbackObj = new FConnectLoginCallback(AsWeak());
						CallbackObj->CallbackLambda = [LocalUserNum, AccountId, AccountCredentials, this](const EOS_Connect_LoginCallbackInfo* Data)
						{
							if (Data->ResultCode == EOS_EResult::EOS_Success)
							{
								// We have an account mapping, skip to final login
								FullLoginCallback(LocalUserNum, AccountId, Data->LocalUserId, AccountCredentials);
							}
							else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
							{
								// We need to create the mapping for this user using the continuation token
								CreateConnectedLogin(LocalUserNum, AccountId, Data->ContinuanceToken, AccountCredentials);
							}
							else
							{
								UE_LOG_ONLINE(Error, TEXT("ConnectLogin(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
								Logout(LocalUserNum);
							}
						};
						EOS_Connect_Login(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

						EOS_Auth_Token_Release(EOSAuthToken);
					}
					else
					{
						UE_LOG_ONLINE(Error, TEXT("ConnectLogin(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(CopyResult)));
						Logout(LocalUserNum);
					}
#if ADD_USER_LOGIN_INFO
				}
			}));
#endif

	return true;
}

void FUserManagerEOS::RefreshConnectLogin(int32 LocalUserNum)
{
	const EOS_EpicAccountId AccountId = GetLocalEpicAccountId(LocalUserNum);
	if (!AccountId)
	{
		UE_LOG_ONLINE(Error, TEXT("Can't refresh ConnectLogin(%d) since (%d) is not logged in"), LocalUserNum, LocalUserNum);
		return;
	}

	const FEOSSettings Settings = UEOSSettings::GetSettings();
	// In the case where bIsDefaultOSS is true, FUserManagerEOS::Login will default to using EOS_Auth_Login regardless of the value that bUseEAS is set to
	// This behaviour will be fixed as part of a wider refactor of FUserManagerEOS::Login
	const bool bShouldUseEOSAuthToken = EOSSubsystem->bIsDefaultOSS || Settings.bUseEAS;
	if (bShouldUseEOSAuthToken)
	{
		const FString AccessToken = GetAuthToken(LocalUserNum);
		if (!AccessToken.IsEmpty())
		{
			// We update the auth token cached in the user account, along with the user information
			const FUserOnlineAccountEOSRef UserAccountRef = GetLocalUserChecked(LocalUserNum).UserOnlineAccount.ToSharedRef();
			UpdateUserInfo(UserAccountRef, AccountId, AccountId);

			EOS_Connect_Credentials Credentials = { };
			Credentials.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_CREDENTIALS_API_LATEST, 1);
			Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;
			auto AccessTokenConverter = StringCast<UTF8CHAR>(*AccessToken);
			Credentials.Token = (const char*)AccessTokenConverter.Get();

			EOS_Connect_LoginOptions Options = { };
			Options.ApiVersion = 2;
			UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_LOGIN_API_LATEST, 2);
			Options.Credentials = &Credentials;

			FConnectLoginCallback* CallbackObj = new FConnectLoginCallback(AsWeak());
			CallbackObj->CallbackLambda = [LocalUserNum, AccountId, this](const EOS_Connect_LoginCallbackInfo* Data)
			{
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG_ONLINE(Error, TEXT("Failed to refresh ConnectLogin(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
					Logout(LocalUserNum);
				}
			};
			EOS_Connect_Login(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("[FUserManagerEOS::RefreshConnectLogin] AccessToken for user [%d] is empty"), LocalUserNum);
			Logout(LocalUserNum);
		}
	}
	else
	{
		// Not using EAS so grab the platform auth token
		GetPlatformAuthToken(LocalUserNum,
			FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda([this, WeakThis = AsWeak()](int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken)
			{
				if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
				{
					if (!bWasSuccessful || !AuthToken.IsValid())
					{
						UE_LOG_ONLINE(Error, TEXT("ConnectLoginNoEAS(%d) failed due to the platform OSS giving an empty auth token"), LocalUserNum);
						Logout(LocalUserNum);
						return;
					}

					// Now login into our EOS account
					const FOnlineAccountCredentials& Creds = *GetLocalUserChecked(LocalUserNum).LastLoginCredentials;
					EOS_EExternalCredentialType CredType = ToEOS_EExternalCredentialType(GetPlatformOSS()->GetSubsystemName(), Creds);
					FConnectCredentials Credentials(CredType, AuthToken);
					EOS_Connect_LoginOptions Options = { };
					Options.ApiVersion = 2;
					UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_LOGIN_API_LATEST, 2);
					Options.Credentials = &Credentials;

					FConnectLoginCallback* CallbackObj = new FConnectLoginCallback(AsWeak());
					CallbackObj->CallbackLambda = [this, LocalUserNum](const EOS_Connect_LoginCallbackInfo* Data)
					{
						if (Data->ResultCode != EOS_EResult::EOS_Success)
						{
							UE_LOG_ONLINE(Error, TEXT("Failed to refresh ConnectLogin(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
							Logout(LocalUserNum);
						}
					};
					EOS_Connect_Login(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
				}
			}));
	}
}

typedef TEOSCallback<EOS_Connect_OnCreateUserCallback, EOS_Connect_CreateUserCallbackInfo, FUserManagerEOS> FCreateUserCallback;

void FUserManagerEOS::CreateConnectedLogin(int32 LocalUserNum, EOS_EpicAccountId AccountId, EOS_ContinuanceToken Token, const FOnlineAccountCredentials& AccountCredentials)
{
	EOS_Connect_CreateUserOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_CREATEUSER_API_LATEST, 1);
	Options.ContinuanceToken = Token;

	FCreateUserCallback* CallbackObj = new FCreateUserCallback(AsWeak());
	CallbackObj->CallbackLambda = [LocalUserNum, AccountId, AccountCredentials, this](const EOS_Connect_CreateUserCallbackInfo* Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			FullLoginCallback(LocalUserNum, AccountId, Data->LocalUserId, AccountCredentials);
		}
		else
		{
// @todo joeg - logout?
			FString ErrorString = FString::Printf(TEXT("Login(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdEOS::EmptyId(), ErrorString);
		}
	};
	EOS_Connect_CreateUser(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
}

typedef TEOSGlobalCallback<EOS_Connect_OnAuthExpirationCallback, EOS_Connect_AuthExpirationCallbackInfo, FUserManagerEOS> FRefreshAuthCallback;
typedef TEOSGlobalCallback<EOS_Presence_OnPresenceChangedCallback, EOS_Presence_PresenceChangedCallbackInfo, FUserManagerEOS> FPresenceChangedCallback;
typedef TEOSGlobalCallback<EOS_Friends_OnFriendsUpdateCallback, EOS_Friends_OnFriendsUpdateInfo, FUserManagerEOS> FFriendsStatusUpdateCallback;
typedef TEOSGlobalCallback<EOS_Auth_OnLoginStatusChangedCallback, EOS_Auth_LoginStatusChangedCallbackInfo, FUserManagerEOS> FLoginStatusChangedCallback;

void FUserManagerEOS::FullLoginCallback(int32 LocalUserNum, EOS_EpicAccountId AccountId, EOS_ProductUserId UserId, const FOnlineAccountCredentials& AccountCredentials)
{
	// Add our login status changed callback if not already set
	if (LoginNotificationId == 0)
	{
		FLoginStatusChangedCallback* CallbackObj = new FLoginStatusChangedCallback(AsWeak());
		LoginNotificationCallback = CallbackObj;
		CallbackObj->CallbackLambda = [this](const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
		{
			LoginStatusChanged(Data);
		};

		EOS_Auth_AddNotifyLoginStatusChangedOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST, 1);
		LoginNotificationId = EOS_Auth_AddNotifyLoginStatusChanged(EOSSubsystem->AuthHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}
	// Register for friends updates if not set yet
	if (FriendsNotificationId == 0)
	{
		FFriendsStatusUpdateCallback* CallbackObj = new FFriendsStatusUpdateCallback(AsWeak());
		FriendsNotificationCallback = CallbackObj;
		CallbackObj->CallbackLambda = [LocalUserNum, this](const EOS_Friends_OnFriendsUpdateInfo* Data)
		{
			FriendStatusChanged(Data);
		};

		EOS_Friends_AddNotifyFriendsUpdateOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST, 1);
		FriendsNotificationId = EOS_Friends_AddNotifyFriendsUpdate(EOSSubsystem->FriendsHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}
	// Register for presence updates if not set yet
	if (PresenceNotificationId == 0)
	{
		FPresenceChangedCallback* CallbackObj = new FPresenceChangedCallback(AsWeak());
		PresenceNotificationCallback = CallbackObj;
		CallbackObj->CallbackLambda = [LocalUserNum, this](const EOS_Presence_PresenceChangedCallbackInfo* Data)
		{
			if (FUniqueNetIdEOSRegistry::Find(Data->PresenceUserId))
			{
				// Update the presence data to the most recent
				UpdatePresence(LocalUserNum, Data->PresenceUserId);
				return;
			}
		};

		EOS_Presence_AddNotifyOnPresenceChangedOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_ADDNOTIFYONPRESENCECHANGED_API_LATEST, 1);
		PresenceNotificationId = EOS_Presence_AddNotifyOnPresenceChanged(EOSSubsystem->PresenceHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}

	// We add the local user
	const FLocalUserEOS& LocalUser = AddLocalUser(LocalUserNum, AccountId, UserId, AccountCredentials);

	TriggerOnLoginCompleteDelegates(LocalUserNum, true, *LocalUser.UniqueNetId, FString());
	TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *LocalUser.UniqueNetId);
}

typedef TEOSCallback<EOS_Auth_OnLogoutCallback, EOS_Auth_LogoutCallbackInfo, FUserManagerEOS> FLogoutCallback;

bool FUserManagerEOS::Logout(int32 LocalUserNum)
{
	FUniqueNetIdEOSPtr UserId = GetLocalUniqueNetIdEOS(LocalUserNum);
	if (!UserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."),
			LocalUserNum);
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		return false;
	}

	FLogoutCallback* CallbackObj = new FLogoutCallback(AsWeak());
	CallbackObj->CallbackLambda = [LocalUserNum, this](const EOS_Auth_LogoutCallbackInfo* Data)
	{
		FDeletePersistentAuthCallback* DeleteAuthCallbackObj = new FDeletePersistentAuthCallback(AsWeak());
		DeleteAuthCallbackObj->CallbackLambda = [this, LocalUserNum, LogoutResultCode = Data->ResultCode](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			if (LogoutResultCode == EOS_EResult::EOS_Success)
			{
				RemoveLocalUser(LocalUserNum);

				TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
			}
			else
			{
				TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
			}
		};

		EOS_Auth_DeletePersistentAuthOptions DeletePersistentAuthOptions;
		DeletePersistentAuthOptions.ApiVersion = 2;
		UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST, 2);
		DeletePersistentAuthOptions.RefreshToken = nullptr;
		EOS_Auth_DeletePersistentAuth(EOSSubsystem->AuthHandle, &DeletePersistentAuthOptions, (void*)DeleteAuthCallbackObj, DeleteAuthCallbackObj->GetCallbackPtr());
	};

	EOS_Auth_LogoutOptions LogoutOptions = { };
	LogoutOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LOGOUT_API_LATEST, 1);
	LogoutOptions.LocalUserId = UserId->GetEpicAccountId();

	EOS_Auth_Logout(EOSSubsystem->AuthHandle, &LogoutOptions, CallbackObj, CallbackObj->GetCallbackPtr());

	return true;
}

bool FUserManagerEOS::AutoLogin(int32 LocalUserNum)
{
	FString LoginId;
	FString Password;
	FString AuthType;

	FParse::Value(FCommandLine::Get(), TEXT("AUTH_LOGIN="), LoginId);
	FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), Password);
	FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), AuthType);

	FEOSSettings Settings = UEOSSettings::GetSettings();

	if (EOSSubsystem->bIsDefaultOSS && Settings.bUseEAS && AuthType.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to AutoLogin user (%d) due to missing auth command line args"), LocalUserNum);
		return false;
	}
	FOnlineAccountCredentials Creds(AuthType, LoginId, Password);

	return Login(LocalUserNum, Creds);
}

FLocalUserEOS& FUserManagerEOS::AddLocalUser(int32 LocalUserNum, EOS_EpicAccountId EpicAccountId, EOS_ProductUserId UserId, const FOnlineAccountCredentials& AccountCredentials)
{
	// Set the default user to the first one that logs in
	if (DefaultLocalUser == INVALID_LOCAL_USER)
	{
		DefaultLocalUser = LocalUserNum;
	}

	// Init player lists
	if (!LocalUsers.IsValidIndex(LocalUserNum))
	{
		LocalUsers.Insert(LocalUserNum, FLocalUserEOS());
	}
	FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);

	FUniqueNetIdEOSRef UserNetId = FUniqueNetIdEOSRegistry::FindOrAdd(EpicAccountId, UserId);
	LocalUser.UniqueNetId = UserNetId;

	FUserOnlineAccountEOSRef UserAccountRef(new FUserOnlineAccountEOS(UserNetId, *EOSSubsystem));
	LocalUser.UserOnlineAccount = UserAccountRef;

	LocalUser.FriendsList = MakeShareable(new FFriendsListEOS(LocalUserNum, UserNetId));

	LocalUser.LastLoginCredentials = MakeShared<FOnlineAccountCredentials>(AccountCredentials);

	// Add auth refresh notification if not set for this user yet
	if (!LocalUser.ConnectLoginNotification.IsValid())
	{
		FNotificationIdCallbackPairPtr NotificationPairPtr = MakeShared<FNotificationIdCallbackPair>();
		LocalUser.ConnectLoginNotification = NotificationPairPtr;

		FRefreshAuthCallback* CallbackObj = new FRefreshAuthCallback(AsWeak());
		NotificationPairPtr->Callback = CallbackObj;
		CallbackObj->CallbackLambda = [LocalUserNum, this](const EOS_Connect_AuthExpirationCallbackInfo* Data)
		{
			RefreshConnectLogin(LocalUserNum);
		};

		EOS_Connect_AddNotifyAuthExpirationOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST, 1);
		NotificationPairPtr->NotificationId = EOS_Connect_AddNotifyAuthExpiration(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}

	const FOnlineUserEOSRef UserRef(new FOnlineUserEOS(UserNetId, *EOSSubsystem));
	UniqueNetIdToUserRefMap.Emplace(UserNetId, UserRef);

	// Once all the fields are set, we start gathering additional information
	ReadFriendsList(LocalUserNum, FString());

	// Update user info (display name, country, language)
	UpdateUserInfo(UserAccountRef, EpicAccountId, EpicAccountId);

	return LocalUser;
}

FString FUserManagerEOS::GetBestDisplayName(EOS_EpicAccountId TargetUserId, const FStringView& RequestedPlatform) const
{
	FString Result;

	EOS_UserInfo_BestDisplayName* EosBestDisplayName = nullptr;
	EOS_EResult BestDisplayNameResult = EOS_EResult::EOS_Success;

	const EOS_EpicAccountId LocalUserId = GetLocalEpicAccountId(GetDefaultLocalUser());

	if (RequestedPlatform.IsEmpty())
	{
		EOS_UserInfo_CopyBestDisplayNameOptions BestDisplayNameOptions = { };
		BestDisplayNameOptions.ApiVersion = 1; 
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST, 1);
		BestDisplayNameOptions.LocalUserId = LocalUserId;
		BestDisplayNameOptions.TargetUserId = TargetUserId;

		BestDisplayNameResult = EOS_UserInfo_CopyBestDisplayName(EOSSubsystem->UserInfoHandle, &BestDisplayNameOptions, &EosBestDisplayName);
	}

	if (!RequestedPlatform.IsEmpty() || BestDisplayNameResult == EOS_EResult::EOS_UserInfo_BestDisplayNameIndeterminate)
	{
		EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions BestDisplayNameWithPlatformOptions = {};
		BestDisplayNameWithPlatformOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST, 1);
		BestDisplayNameWithPlatformOptions.LocalUserId = LocalUserId;
		BestDisplayNameWithPlatformOptions.TargetUserId = TargetUserId;
		
		BestDisplayNameWithPlatformOptions.TargetPlatformType = EOS_OPT_Epic;
		if (!RequestedPlatform.IsEmpty())
		{
			BestDisplayNameWithPlatformOptions.TargetPlatformType = EOSOnlinePlatformTypeFromString(RequestedPlatform);
		}

		BestDisplayNameResult = EOS_UserInfo_CopyBestDisplayNameWithPlatform(EOSSubsystem->UserInfoHandle, &BestDisplayNameWithPlatformOptions, &EosBestDisplayName);
	}

	if (EosBestDisplayName)
	{
		Result = GetBestDisplayNameStr(*EosBestDisplayName);
		EOS_UserInfo_BestDisplayName_Release(EosBestDisplayName);
	}

	return Result;
}

void FUserManagerEOS::UpdateUserInfo(IAttributeAccessInterfaceRef AttributeAccessRef, EOS_EpicAccountId LocalId, EOS_EpicAccountId AccountId)
{
	EOS_UserInfo_CopyUserInfoOptions Options = { };
	Options.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYUSERINFO_API_LATEST, 3);
	Options.LocalUserId = LocalId;
	Options.TargetUserId = AccountId;

	EOS_UserInfo* UserInfo = nullptr;

	const EOS_EResult CopyResult = EOS_UserInfo_CopyUserInfo(EOSSubsystem->UserInfoHandle, &Options, &UserInfo);
	UE_CLOG_ONLINE(CopyResult != EOS_EResult::EOS_Success, Warning, TEXT("%hs Result=[%s]"), __FUNCTION__, *LexToString(CopyResult));
	if (CopyResult == EOS_EResult::EOS_Success)
	{
		AttributeAccessRef->SetInternalAttribute(USER_ATTR_COUNTRY, UTF8_TO_TCHAR(UserInfo->Country));
		AttributeAccessRef->SetInternalAttribute(USER_ATTR_LANG, UTF8_TO_TCHAR(UserInfo->PreferredLanguage));
		EOS_UserInfo_Release(UserInfo);
	}
}

TSharedPtr<FUserOnlineAccount> FUserManagerEOS::GetUserAccount(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> Result;

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			const FLocalUserEOS& LocalUser = LocalUsers[Index];

			if (*LocalUser.UniqueNetId == UserId && LocalUser.UserOnlineAccount.IsValid())
			{
				Result = LocalUser.UserOnlineAccount;
			}
		}
	}

	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount>> FUserManagerEOS::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Result;

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			if (const FUserOnlineAccountEOSPtr& UserOnlineAccount = LocalUsers[Index].UserOnlineAccount)
			{
				Result.Add(UserOnlineAccount);
			}
		}
	}

	return Result;
}

FUniqueNetIdPtr FUserManagerEOS::GetUniquePlayerId(int32 LocalUserNum) const
{
	return GetLocalUniqueNetIdEOS(LocalUserNum);
}

const FUniqueNetIdEOSPtr FUserManagerEOS::GetLocalUniqueNetIdEOS(int32 LocalUserNum) const
{
	if (LocalUsers.IsValidIndex(LocalUserNum))
	{
		return LocalUsers[LocalUserNum].UniqueNetId;
	}

	if (IsRunningDedicatedServer())
	{
		return FUniqueNetIdEOS::EmptyId();
	}

	return nullptr;
}

EOS_EpicAccountId FUserManagerEOS::GetLocalEpicAccountId(int32 LocalUserNum) const
{
	if (FUniqueNetIdEOSPtr UniqueNetId = GetLocalUniqueNetIdEOS(LocalUserNum))
	{
		return UniqueNetId->GetEpicAccountId();
	}

	return nullptr;
}

EOS_ProductUserId FUserManagerEOS::GetLocalProductUserId(int32 LocalUserNum) const
{
	if (FUniqueNetIdEOSPtr UniqueNetId = GetLocalUniqueNetIdEOS(LocalUserNum))
	{
		return UniqueNetId->GetProductUserId();
	}

	return nullptr;
}

int32 FUserManagerEOS::GetLocalUserNumFromUniqueNetId(const FUniqueNetId& NetId) const
{
	const FUniqueNetIdEOS& EosId = FUniqueNetIdEOS::Cast(NetId);

	for (int32 LocalUserNum = 0; LocalUserNum < LocalUsers.GetMaxIndex(); LocalUserNum++)
	{
		if (LocalUsers.IsValidIndex(LocalUserNum))
		{
			const FUniqueNetIdEOSPtr& Entry = LocalUsers[LocalUserNum].UniqueNetId;

			if (*Entry == EosId)
			{
				return LocalUserNum;
			}
		}
	}

	return INVALID_LOCAL_USER;
}

int32 FUserManagerEOS::GetLocalUserNumFromEpicAccountId(const EOS_EpicAccountId& EpicAccountId) const
{
	for (int32 LocalUserNum = 0; LocalUserNum < LocalUsers.GetMaxIndex(); LocalUserNum++)
	{
		if (LocalUsers.IsValidIndex(LocalUserNum))
		{
			const FUniqueNetIdEOSPtr& Entry = LocalUsers[LocalUserNum].UniqueNetId;

			if (Entry->GetEpicAccountId() == EpicAccountId)
			{
				return LocalUserNum;
			}
		}
	}

	return INVALID_LOCAL_USER;
}

int32 FUserManagerEOS::GetLocalUserNumFromProductUserId(const EOS_ProductUserId& ProductUserId) const
{
	for (int32 LocalUserNum = 0; LocalUserNum < LocalUsers.GetMaxIndex(); LocalUserNum++)
	{
		if (LocalUsers.IsValidIndex(LocalUserNum))
		{
			const FUniqueNetIdEOSPtr& Entry = LocalUsers[LocalUserNum].UniqueNetId;

			if (Entry->GetProductUserId() == ProductUserId)
			{
				return LocalUserNum;
			}
		}
	}

	return INVALID_LOCAL_USER;
}

const FUniqueNetIdEOSPtr FUserManagerEOS::GetLocalUniqueNetIdEOS(const EOS_EpicAccountId& EpicAccountId) const
{
	for (int32 LocalUserNum = 0; LocalUserNum < LocalUsers.GetMaxIndex(); LocalUserNum++)
	{
		if (LocalUsers.IsValidIndex(LocalUserNum))
		{
			const FUniqueNetIdEOSPtr& Entry = LocalUsers[LocalUserNum].UniqueNetId;

			if (Entry->GetEpicAccountId() == EpicAccountId)
			{
				return Entry;
			}
		}
	}

	return nullptr;
}

const FUniqueNetIdEOSPtr FUserManagerEOS::GetLocalUniqueNetIdEOS(const EOS_ProductUserId& ProductUserId) const
{
	for (int32 LocalUserNum = 0; LocalUserNum < LocalUsers.GetMaxIndex(); LocalUserNum++)
	{
		if (LocalUsers.IsValidIndex(LocalUserNum))
		{
			const FUniqueNetIdEOSPtr& Entry = LocalUsers[LocalUserNum].UniqueNetId;

			if (Entry->GetProductUserId() == ProductUserId)
			{
				return Entry;
			}
		}
	}

	return nullptr;
}

bool FUserManagerEOS::IsLocalUser(const FUniqueNetId& NetId) const
{
	return GetLocalUserNumFromUniqueNetId(NetId) != INVALID_LOCAL_USER;
}

bool FUserManagerEOS::IsLocalUser(const EOS_EpicAccountId& EpicAccountId) const
{
	return GetLocalUserNumFromEpicAccountId(EpicAccountId) != INVALID_LOCAL_USER;
}

bool FUserManagerEOS::IsLocalUser(const EOS_ProductUserId& ProductUserId) const
{
	return GetLocalUserNumFromProductUserId(ProductUserId) != INVALID_LOCAL_USER;
}

typedef TEOSCallback<EOS_Connect_OnQueryProductUserIdMappingsCallback, EOS_Connect_QueryProductUserIdMappingsCallbackInfo, FUserManagerEOS> FConnectQueryProductUserIdMappingsCallback;

/**
 * Uses the Connect API to retrieve the EOS_EpicAccountId for a given EOS_ProductUserId
 *
 * @param ProductUserId the product user id we want to query
 * @Param OutEpicAccountId the epic account id we will assign if the query is successful
 *
 * @return true if the operation was successful, false otherwise
 */
bool FUserManagerEOS::GetEpicAccountIdFromProductUserId(int32 LocalUserNum, const EOS_ProductUserId& ProductUserId, EOS_EpicAccountId& OutEpicAccountId) const
{
	bool bResult = false;

	char EpicIdStr[EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH+1];
	int32 EpicIdStrSize = sizeof(EpicIdStr);

	EOS_Connect_GetProductUserIdMappingOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_GETPRODUCTUSERIDMAPPING_API_LATEST, 1);
	Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
	Options.LocalUserId = GetLocalProductUserId(LocalUserNum);
	Options.TargetProductUserId = ProductUserId;

	EOS_EResult Result = EOS_Connect_GetProductUserIdMapping(EOSSubsystem->ConnectHandle, &Options, EpicIdStr, &EpicIdStrSize);
	if (Result == EOS_EResult::EOS_Success)
	{
		OutEpicAccountId = EOS_EpicAccountId_FromString(EpicIdStr);
		bResult = true;
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("[FUserManagerEOS::GetEpicAccountIdFromProductUserId] EOS_Connect_GetProductUserIdMapping not successful for ProductUserId (%s). Finished with EOS_EResult %s"), *LexToString(ProductUserId), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}

	return bResult;
}

void FUserManagerEOS::ResolveUniqueNetId(int32 LocalUserNum, const EOS_ProductUserId& ProductUserId, const FResolveUniqueNetIdCallback& Callback) const
{
	TArray<EOS_ProductUserId> ProductUserIds = { const_cast<EOS_ProductUserId>(ProductUserId) };

	FResolveUniqueNetIdsCallback GroupCallback = [ProductUserId, OriginalCallback = Callback](TMap<EOS_ProductUserId, FUniqueNetIdEOSRef> ResolvedUniqueNetIds) {
		OriginalCallback(ResolvedUniqueNetIds[ProductUserId]);
	};

	ResolveUniqueNetIds(LocalUserNum, ProductUserIds, GroupCallback);
}

void FUserManagerEOS::ResolveUniqueNetIds(int32 LocalUserNum, const TArray<EOS_ProductUserId>& ProductUserIds, const FResolveUniqueNetIdsCallback& Callback) const
{
	TMap<EOS_ProductUserId, FUniqueNetIdEOSRef> ResolvedUniqueNetIds;
	TArray<EOS_ProductUserId> ProductUserIdsToResolve;

	for (const EOS_ProductUserId& ProductUserId : ProductUserIds)
	{
		EOS_EpicAccountId EpicAccountId;

		// We check first if the Product User Id has already been queried, which would allow us to retrieve its Epic Account Id directly
		if (GetEpicAccountIdFromProductUserId(LocalUserNum, ProductUserId, EpicAccountId))
		{
			const FUniqueNetIdEOSRef UniqueNetId = FUniqueNetIdEOSRegistry::FindOrAdd(EpicAccountId, ProductUserId);

			ResolvedUniqueNetIds.Add(ProductUserId, UniqueNetId);
		}
		else
		{
			// If that's not the case, we'll have to query them first
			ProductUserIdsToResolve.Add(ProductUserId);
		}
	}

	if (!ProductUserIdsToResolve.IsEmpty())
	{
		EOS_Connect_QueryProductUserIdMappingsOptions QueryProductUserIdMappingsOptions = {};
		QueryProductUserIdMappingsOptions.ApiVersion = 2;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST, 2);
		QueryProductUserIdMappingsOptions.LocalUserId = EOSSubsystem->UserManager->GetLocalProductUserId(0);
		QueryProductUserIdMappingsOptions.ProductUserIds = ProductUserIdsToResolve.GetData();
		QueryProductUserIdMappingsOptions.ProductUserIdCount = ProductUserIdsToResolve.Num();

		FConnectQueryProductUserIdMappingsCallback* CallbackObj = new FConnectQueryProductUserIdMappingsCallback(FUserManagerEOSConstWeakPtr(AsShared()));
		CallbackObj->CallbackLambda = [this, LocalUserNum, ProductUserIdsToResolve, ResolvedUniqueNetIds = MoveTemp(ResolvedUniqueNetIds), Callback](const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data) mutable
		{
			if (Data->ResultCode != EOS_EResult::EOS_Success)
			{
				UE_LOG_ONLINE(Verbose, TEXT("[FUserManagerEOS::ResolveUniqueNetIds] EOS_Connect_QueryProductUserIdMappings not successful for user (%s). Finished with EOS_EResult %s."), *LexToString(Data->LocalUserId), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}

			for (const EOS_ProductUserId& ProductUserId : ProductUserIdsToResolve)
			{
				EOS_EpicAccountId EpicAccountId = nullptr;

				GetEpicAccountIdFromProductUserId(LocalUserNum, ProductUserId, EpicAccountId);

				const FUniqueNetIdEOSRef UniqueNetId = FUniqueNetIdEOSRegistry::FindOrAdd(EpicAccountId, ProductUserId);

				ResolvedUniqueNetIds.Add(ProductUserId, UniqueNetId);
			}

			Callback(ResolvedUniqueNetIds);
		};

		EOS_Connect_QueryProductUserIdMappings(EOSSubsystem->ConnectHandle, &QueryProductUserIdMappingsOptions, CallbackObj, CallbackObj->GetCallbackPtr());
	}
	else
	{
		EOSSubsystem->ExecuteNextTick([Callback, ResolvedUniqueNetIds]()
			{
				Callback(ResolvedUniqueNetIds);
			});
	}
}

void FUserManagerEOS::ResolveUniqueNetId(int32 LocalUserNum, const EOS_EpicAccountId& EpicAccountId, const FResolveUniqueNetIdCallback& Callback)
{
	const FResolveEpicAccountIdsCallback GroupCallback = [EpicAccountId, OriginalCallback = Callback](bool bWasSuccessful, TMap<EOS_EpicAccountId, FUniqueNetIdEOSRef> ResolvedUniqueNetIds, FString ErrorStr) {
		OriginalCallback(ResolvedUniqueNetIds[EpicAccountId]);
	};

	ResolveUniqueNetIds(LocalUserNum, { EpicAccountId }, GroupCallback);
}

void FUserManagerEOS::ResolveUniqueNetIds(int32 LocalUserNum, const TArray<EOS_EpicAccountId>& EpicAccountIds, const FResolveEpicAccountIdsCallback& Callback)
{
	TMap<EOS_EpicAccountId, FUniqueNetIdEOSRef> ResolvedUniqueNetIds;
	TArray<FString> EpicAccountIdsToResolve;

	for (const EOS_EpicAccountId& EpicAccountId : EpicAccountIds)
	{
		if (ensure(EOS_EpicAccountId_IsValid(EpicAccountId) == EOS_TRUE))
		{
			// We check first if the id is already registered, which would allow us to retrieve it directly
			if (FUniqueNetIdEOSPtr UniqueNetId = FUniqueNetIdEOSRegistry::Find(EpicAccountId))
			{
				ResolvedUniqueNetIds.Add(EpicAccountId, UniqueNetId.ToSharedRef());
			}
			else
			{
				// If that's not the case, we'll have to query them first
				EpicAccountIdsToResolve.Add(LexToString(EpicAccountId));
			}
		}
	}

	if (!EpicAccountIdsToResolve.IsEmpty())
	{
		QueryExternalIdMappings(*GetLocalUniqueNetIdEOS(LocalUserNum), FExternalIdQueryOptions(), EpicAccountIdsToResolve, FOnQueryExternalIdMappingsComplete::CreateLambda([this, ResolvedUniqueNetIds = MoveTemp(ResolvedUniqueNetIds), Callback](bool bWasSuccessful, const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FString& Error) mutable
			{
				if (bWasSuccessful)
				{
					for (const FString& ExternalId : ExternalIds)
					{
						EOS_EpicAccountId AccountId;
						LexFromString(AccountId, *ExternalId);

						if (EOS_EpicAccountId_IsValid(AccountId) == EOS_TRUE)
						{
							ResolvedUniqueNetIds.Add(AccountId, FUniqueNetIdEOSRegistry::FindChecked(AccountId));
						}
					}
				}

				Callback(bWasSuccessful, ResolvedUniqueNetIds, Error);
			}));
	}
	else
	{
		EOSSubsystem->ExecuteNextTick([Callback, ResolvedUniqueNetIds = MoveTemp(ResolvedUniqueNetIds)]()
			{
				Callback(true, ResolvedUniqueNetIds, FString());
			});
	}
}

FOnlineUserPtr FUserManagerEOS::GetLocalOnlineUser(int32 LocalUserNum) const
{
	FOnlineUserPtr OnlineUser;

	if (LocalUsers.IsValidIndex(LocalUserNum))
	{
		const FLocalUserEOS& LocalUser = LocalUsers[LocalUserNum];

		if (LocalUser.UserOnlineAccount.IsValid())
		{
			OnlineUser = LocalUser.UserOnlineAccount;
		}
	}

	return OnlineUser;
}

FOnlineUserPtr FUserManagerEOS::GetOnlineUser(EOS_ProductUserId UserId) const
{
	FOnlineUserPtr OnlineUser;

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			const FLocalUserEOS& LocalUser = LocalUsers[Index];

			if (LocalUser.UniqueNetId->GetProductUserId() == UserId && LocalUser.UserOnlineAccount.IsValid())
			{
				OnlineUser = LocalUser.UserOnlineAccount;
			}
		}
	}

	return OnlineUser;	
}

FOnlineUserPtr FUserManagerEOS::GetOnlineUser(EOS_EpicAccountId AccountId) const
{
	FOnlineUserPtr OnlineUser;

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			const FLocalUserEOS& LocalUser = LocalUsers[Index];

			if (LocalUser.UniqueNetId->GetEpicAccountId() == AccountId && LocalUser.UserOnlineAccount.IsValid())
			{
				OnlineUser = LocalUser.UserOnlineAccount;
			}
		}
	}

	return OnlineUser;
}

void FUserManagerEOS::RemoveLocalUser(int32 LocalUserNum)
{
	if (LocalUsers.IsValidIndex(LocalUserNum))
	{
		const FUniqueNetIdEOSRef FoundId = GetLocalUserChecked(LocalUserNum).UniqueNetId.ToSharedRef();

		EOSSubsystem->ReleaseVoiceChatUserInterface(*FoundId);

		LocalUsers.RemoveAt(LocalUserNum);
	}

	// Reset this for the next user login
	if (LocalUserNum == DefaultLocalUser)
	{
		DefaultLocalUser = INVALID_LOCAL_USER;
	}
}

FUniqueNetIdPtr FUserManagerEOS::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	// If we're passed an EOSPlus id, the first EOS_ID_BYTE_SIZE bytes are the EAS|EOS part we care about.
	Size = FMath::Min(Size, EOS_ID_BYTE_SIZE);
	return FUniqueNetIdEOSRegistry::FindOrAdd(Bytes, Size);
}

FUniqueNetIdPtr FUserManagerEOS::CreateUniquePlayerId(const FString& InStr)
{
	FString NetIdStr = InStr;
	// If we're passed an EOSPlus id, remove the platform id and separator.
	NetIdStr.Split(EOSPLUS_ID_SEPARATOR, nullptr, &NetIdStr);
	return FUniqueNetIdEOSRegistry::FindOrAdd(NetIdStr);
}

ELoginStatus::Type FUserManagerEOS::GetLoginStatus(int32 LocalUserNum) const
{
	FUniqueNetIdEOSPtr UserId = GetLocalUniqueNetIdEOS(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FUserManagerEOS::GetLoginStatus(const FUniqueNetIdEOS& UserId) const
{
	FEOSSettings Settings = UEOSSettings::GetSettings();
	// If the user isn't using EAS, then only check for a product user id
	if (!Settings.bUseEAS)
	{
		const EOS_ProductUserId ProductUserId = UserId.GetProductUserId();
		if (ProductUserId != nullptr)
		{
			return ELoginStatus::LoggedIn;
		}
		return ELoginStatus::NotLoggedIn;
	}

	const EOS_EpicAccountId AccountId = UserId.GetEpicAccountId();
	if (AccountId == nullptr)
	{
		return ELoginStatus::NotLoggedIn;
	}

	EOS_ELoginStatus LoginStatus = EOS_Auth_GetLoginStatus(EOSSubsystem->AuthHandle, AccountId);
	switch (LoginStatus)
	{
		case EOS_ELoginStatus::EOS_LS_LoggedIn:
		{
			return ELoginStatus::LoggedIn;
		}
		case EOS_ELoginStatus::EOS_LS_UsingLocalProfile:
		{
			return ELoginStatus::UsingLocalProfile;
		}
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FUserManagerEOS::GetLoginStatus(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdEOS& EosId = FUniqueNetIdEOS::Cast(UserId);
	return GetLoginStatus(EosId);
}

FString FUserManagerEOS::GetPlayerNickname(int32 LocalUserNum) const
{
	const FUniqueNetIdEOSPtr UserId = GetLocalUniqueNetIdEOS(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetPlayerNickname(*UserId);
	}

	return FString();
}

FString FUserManagerEOS::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	// GetUserAccount only searches local users
	const TSharedPtr<FUserOnlineAccount> LocalUserAccount = GetUserAccount(UserId);
	if (LocalUserAccount.IsValid())
	{
		return LocalUserAccount->GetDisplayName();
	}

	if (const FOnlineUserEOSRef* RemoteUserRef = UniqueNetIdToUserRefMap.Find(UserId.AsShared()))
	{
		return RemoteUserRef->Get().GetDisplayName();
	}

	return FString();
}

FString FUserManagerEOS::GetAuthToken(int32 LocalUserNum) const
{
	const EOS_EpicAccountId AccountId = GetLocalEpicAccountId(LocalUserNum);

	EOS_Auth_Token* AuthToken = nullptr;
	EOS_Auth_CopyUserAuthTokenOptions CopyOptions = { };
	CopyOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

	const EOS_EResult CopyResult = EOS_Auth_CopyUserAuthToken(EOSSubsystem->AuthHandle, &CopyOptions, AccountId, &AuthToken);
	if (CopyResult == EOS_EResult::EOS_Success)
	{
		const FString AuthTokenStr(UTF8_TO_TCHAR(AuthToken->AccessToken));
		EOS_Auth_Token_Release(AuthToken);

		return AuthTokenStr;
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("[FUserManagerEOS::GetAuthToken] EOS_Auth_CopyUserAuthToken failed with EOS result code (%s) for user (%d)"), ANSI_TO_TCHAR(EOS_EResult_ToString(CopyResult)), LocalUserNum);
	}

	return FString();
}

void FUserManagerEOS::RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(LocalUserId, FOnlineError(EOnlineErrorResult::NotImplemented));
}

FPlatformUserId FUserManagerEOS::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	return GetPlatformUserIdFromLocalUserNum(GetLocalUserNumFromUniqueNetId(UniqueNetId));
}

void FUserManagerEOS::GetLinkedAccountAuthToken(int32 LocalUserNum, const FString& /*TokenType*/, const FOnGetLinkedAccountAuthTokenCompleteDelegate& Delegate) const
{
	FExternalAuthToken ExternalToken;
	ExternalToken.TokenString = GetAuthToken(LocalUserNum);
	Delegate.ExecuteIfBound(LocalUserNum, ExternalToken.IsValid(), ExternalToken);
}

void FUserManagerEOS::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FString FUserManagerEOS::GetAuthType() const
{
	return TEXT("epic");
}

// IOnlineExternalUI Interface

bool FUserManagerEOS::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	FPlatformEOSHelpersPtr EOSHelpers = EOSSubsystem->GetEOSHelpers();
	EOSHelpers->PlatformTriggerLoginUI(EOSSubsystem, ControllerIndex, bShowOnlineOnly, bShowSkipButton, Delegate);

	return true;
}

bool FUserManagerEOS::ShowAccountCreationUI(const int ControllerIndex, const FOnAccountCreationUIClosedDelegate& Delegate)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowAccountCreationUI] This method is not implemented."));
	
	EOSSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(ControllerIndex, FOnlineAccountCredentials(), FOnlineError(EOnlineErrorResult::NotImplemented));
		});

	return true;
}

typedef TEOSCallback<EOS_UI_OnShowFriendsCallback, EOS_UI_ShowFriendsCallbackInfo, FUserManagerEOS> FOnShowFriendsCallback;

bool FUserManagerEOS::ShowFriendsUI(int32 LocalUserNum)
{
	EOS_UI_ShowFriendsOptions Options = {};
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_UI_SHOWFRIENDS_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);

	FOnShowFriendsCallback* CallbackObj = new FOnShowFriendsCallback(AsWeak());
	CallbackObj->CallbackLambda = [](const EOS_UI_ShowFriendsCallbackInfo* Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			UE_LOG_ONLINE_EXTERNALUI(VeryVerbose, TEXT("[FUserManagerEOS::ShowFriendsUI] EOS_UI_ShowFriends was successful."));
		}
		else
		{
			UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowFriendsUI] EOS_UI_ShowFriends was not successful. Finished with error %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
		}
	};

	EOS_UI_ShowFriends(EOSSubsystem->UIHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

	return true;
}


bool FUserManagerEOS::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowInviteUI] This method is not implemented."));

	return false;
}

bool FUserManagerEOS::ShowAchievementsUI(int32 LocalUserNum)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowAchievementsUI] This method is not implemented."));

	return false;
}

bool FUserManagerEOS::ShowLeaderboardUI(const FString& LeaderboardName)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowLeaderboardUI] This method is not implemented."));

	return false;
}

bool FUserManagerEOS::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowWebURL] This method is not implemented."));

	if (Delegate.IsBound())
	{
		EOSSubsystem->ExecuteNextTick([Delegate]()
			{
				Delegate.ExecuteIfBound(FString());
			});
		return true;
	}
	return false;
}

bool FUserManagerEOS::CloseWebURL()
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::CloseWebURL] This method is not implemented."));

	return false;
}

bool FUserManagerEOS::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowProfileUI] This method is not implemented."));

	if (Delegate.IsBound())
	{
		EOSSubsystem->ExecuteNextTick([Delegate]()
			{
				Delegate.ExecuteIfBound();
			});
		return true;
	}
	return false;
}

bool FUserManagerEOS::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowAccountUpgradeUI] This method is not implemented."));

	return false;
}

bool FUserManagerEOS::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowStoreUI] This method is not implemented."));

	if (Delegate.IsBound())
	{
		EOSSubsystem->ExecuteNextTick([Delegate]()
			{
				Delegate.ExecuteIfBound(false);
			});
		return true;
	}
	return false;
}

bool FUserManagerEOS::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	UE_LOG_ONLINE_EXTERNALUI(Warning, TEXT("[FUserManagerEOS::ShowSendMessageUI] This method is not implemented."));

	if (Delegate.IsBound())
	{
		EOSSubsystem->ExecuteNextTick([Delegate]()
			{
				Delegate.ExecuteIfBound(false);
			});
		return true;
	}
	return false;
}

// ~IOnlineExternalUI Interface

typedef TEOSCallback<EOS_Friends_OnQueryFriendsCallback, EOS_Friends_QueryFriendsCallbackInfo, FUserManagerEOS> FReadFriendsCallback;

void FUserManagerEOS::FriendStatusChanged(const EOS_Friends_OnFriendsUpdateInfo* Data)
{
	if (IsLocalUser(Data->LocalUserId))
	{
		const int32 LocalUserNum = GetLocalUserNumFromEpicAccountId(Data->LocalUserId);

		// If the remote user for which we just got an update is not registered, we will register it first
		FUniqueNetIdEOSPtr NetId = FUniqueNetIdEOSRegistry::Find(Data->TargetUserId);
		bool bIsNetIdRegistered = NetId.IsValid() ? UniqueNetIdToUserRefMap.Contains(NetId.ToSharedRef()) : false;
		if (!bIsNetIdRegistered)
		{
			FRemoteUserProcessedCallback Callback = [this, LocalUserId = Data->LocalUserId, TargetUserId = Data->TargetUserId, PreviousStatus = Data->PreviousStatus, CurrentStatus = Data->CurrentStatus](bool bWasSuccessful, FUniqueNetIdEOSRef RemotePlayerNetId, const FString& ErrorStr)
			{
				FriendStatusChangedImpl(LocalUserId, TargetUserId, PreviousStatus, CurrentStatus);
			};

			AddRemotePlayer(LocalUserNum, Data->TargetUserId, Callback);
		}
		else
		{
			FriendStatusChangedImpl(Data->LocalUserId, Data->TargetUserId, Data->PreviousStatus, Data->CurrentStatus);
		}
	}
}

void FUserManagerEOS::FriendStatusChangedImpl(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId, EOS_EFriendsStatus PreviousStatus, EOS_EFriendsStatus CurrentStatus)
{
	const int32 LocalUserNum = GetLocalUserNumFromEpicAccountId(LocalUserId);
	const FUniqueNetIdEOSRef LocalEOSID = GetLocalUserChecked(LocalUserNum).UniqueNetId.ToSharedRef();
	const FUniqueNetIdEOSRef& FriendEOSId = FUniqueNetIdEOSRegistry::FindChecked(TargetUserId);

	switch (CurrentStatus)
	{
	case EOS_EFriendsStatus::EOS_FS_NotFriends: // Invite rejections and friend removal
	{
		//User should already be a friend
		FFriendsListEOSPtr& FriendsListRef = GetLocalUserChecked(LocalUserNum).FriendsList;
		FOnlineFriendEOSPtr Friend = FriendsListRef->GetByNetId(FriendEOSId);
		if (Friend.IsValid())
		{
			FriendsListRef->Remove(FriendEOSId, Friend.ToSharedRef());
			Friend->SetInviteStatus(EInviteStatus::Unknown);

			if (PreviousStatus == EOS_EFriendsStatus::EOS_FS_Friends)
			{
				TriggerOnFriendRemovedDelegates(*LocalEOSID, *FriendEOSId);
			}
			else if (PreviousStatus == EOS_EFriendsStatus::EOS_FS_InviteSent || PreviousStatus == EOS_EFriendsStatus::EOS_FS_InviteReceived)
			{
				TriggerOnInviteRejectedDelegates(*LocalEOSID, *FriendEOSId); // We don't have an "OnInviteRejected" event only for the local user
			}
		}
		else
		{
			UE_LOG_ONLINE_FRIEND(Verbose, TEXT("Friend status notification received for user [%d], but remote user [%s] was not previously registered as a friend"), *FriendEOSId->ToString());
		}

		break;
	}
	case EOS_EFriendsStatus::EOS_FS_InviteSent: // Invite sent by local user
	{
		// The only supported case is NotFriends->InviteSent, other combinations are not possible
		check(PreviousStatus == EOS_EFriendsStatus::EOS_FS_NotFriends);

		FOnlineFriendEOSRef FriendRef = AddFriend(LocalUserNum, *FriendEOSId);

		FriendRef->SetInviteStatus(EInviteStatus::PendingOutbound);
		TriggerOnOutgoingInviteSentDelegates(LocalUserNum);

		break;
	}
	case EOS_EFriendsStatus::EOS_FS_InviteReceived: // Invite received by local user
	{
		// The only supported case is NotFriends->InviteReceived, other combinations are not possible
		check(PreviousStatus == EOS_EFriendsStatus::EOS_FS_NotFriends);

		FOnlineFriendEOSRef FriendRef = AddFriend(LocalUserNum, *FriendEOSId);

		FriendRef->SetInviteStatus(EInviteStatus::PendingInbound);
		TriggerOnInviteReceivedDelegates(*LocalEOSID, *FriendEOSId);

		break;
	}
	case EOS_EFriendsStatus::EOS_FS_Friends: // Accepted invites and friend addition. This logic applies to all three PreviousStatus cases
	{
		FOnlineFriendEOSRef FriendRef = AddFriend(LocalUserNum, *FriendEOSId);

		FriendRef->SetInviteStatus(EInviteStatus::Accepted);
		TriggerOnInviteAcceptedDelegates(*LocalEOSID, *FriendEOSId); // We don't have an "OnFriendAdded", all relationships exist in the context of invites. We also don't have a separate "OnInviteAccepted" for the local user.

		break;
	}
	default:
		checkNoEntry();
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::FriendStatusChanged] Unsupported status received as CurrentStatus"));
	}

	TriggerOnFriendsChangeDelegates(LocalUserNum);
}

FOnlineFriendEOSRef FUserManagerEOS::AddFriend(int32 LocalUserNum, const FUniqueNetIdEOS& FriendNetId)
{
	const FOnlineUserEOSRef UserRef = UniqueNetIdToUserRefMap[FriendNetId.AsShared()];
	const FUniqueNetIdEOSRef FriendNetIdEOSRef = StaticCastSharedRef<const FUniqueNetIdEOS>(FriendNetId.AsShared());
	const FOnlineFriendEOSRef FriendRef = MakeShareable(new FOnlineFriendEOS(FriendNetIdEOSRef, UserRef->UserAttributes, *EOSSubsystem));

	GetLocalUserChecked(LocalUserNum).FriendsList->Add(FriendNetId.AsShared(), FriendRef);

	EOS_Friends_GetStatusOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETSTATUS_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = FriendNetId.GetEpicAccountId();
	EOS_EFriendsStatus Status = EOS_Friends_GetStatus(EOSSubsystem->FriendsHandle, &Options);
	
	FriendRef->SetInviteStatus(ToEInviteStatus(Status));

	// Querying the presence of a non-friend would cause an SDK error.
	// Players that sent/received a friend invitation from us still count as "friends", so check
	// our friend relationship here.
	if(Status == EOS_EFriendsStatus::EOS_FS_Friends)
	{
		QueryPresence(FriendNetId, IgnoredPresenceDelegate);
	}

	return FriendRef;
}

void FUserManagerEOS::AddRemotePlayer(int32 LocalUserNum, EOS_EpicAccountId EpicAccountId, const FRemoteUserProcessedCallback& Callback)
{
	const FResolveUniqueNetIdCallback IdResolutionCallback = [this, LocalUserNum, EpicAccountId, Callback](FUniqueNetIdEOSRef ResolvedUniqueNetId) mutable
	{
		const FOnlineUserEOSRef AttributeRef = MakeShareable(new FOnlineUserEOS(ResolvedUniqueNetId, *EOSSubsystem));

		UniqueNetIdToUserRefMap.Emplace(ResolvedUniqueNetId, AttributeRef);

		// Read the user info for this player
		ReadUserInfo(LocalUserNum, EpicAccountId, Callback);
	};

	ResolveUniqueNetId(LocalUserNum, EpicAccountId, IdResolutionCallback);
}

void FUserManagerEOS::AddRemotePlayers(int32 LocalUserNum, TArray<EOS_EpicAccountId> EpicAccountIds, const FRemoteUsersProcessedCallback& Callback)
{
	const FResolveEpicAccountIdsCallback IdResolutionCallback = [this, LocalUserNum, EpicAccountIds, Callback](bool bWasSuccessful, TMap<EOS_EpicAccountId, FUniqueNetIdEOSRef> ResolvedUniqueNetIds, FString ErrorStr) mutable
	{
		for (const TPair<EOS_EpicAccountId, FUniqueNetIdEOSRef>& Entry : ResolvedUniqueNetIds)
		{
			const FOnlineUserEOSRef AttributeRef = MakeShareable(new FOnlineUserEOS(Entry.Value, *EOSSubsystem));

			UniqueNetIdToUserRefMap.Emplace(Entry.Value, AttributeRef);

			const FRemoteUserProcessedCallback ReadUserInfoCallback = [this, LocalUserNum, Callback](bool bWasSuccessful, FUniqueNetIdEOSRef UserNetId, const FString& ErrorStr) mutable
			{
				FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);

				LocalUser.OngoingQueryUserInfoResults.ProcessedIds.Add(UserNetId);

				LocalUser.OngoingQueryUserInfoResults.bAllWasSuccessful &= bWasSuccessful;

				if (!ErrorStr.IsEmpty())
				{
					LocalUser.OngoingQueryUserInfoResults.AllErrorStr += TEXT("/n") + ErrorStr;
				}

				if (LocalUser.OngoingQueryUserInfoAccounts.IsEmpty())
				{
					FLocalUserEOS::FReadUserInfoResults Results = LocalUser.OngoingQueryUserInfoResults;
					LocalUser.OngoingQueryUserInfoResults.Reset();
					
					Callback(Results.bAllWasSuccessful, Results.ProcessedIds, Results.AllErrorStr);
				}
			};

			// Read the user info for this player
			ReadUserInfo(LocalUserNum, Entry.Key, ReadUserInfoCallback);
		}		
	};

	ResolveUniqueNetIds(LocalUserNum, EpicAccountIds, IdResolutionCallback);
}

// IOnlineFriends Interface

bool FUserManagerEOS::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	if (!LocalUsers.IsValidIndex(LocalUserNum))
	{
		const FString ErrorStr = FString::Printf(TEXT("Can't ReadFriendsList() for user (%d) since they are not logged in"), LocalUserNum);
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("%s"), *ErrorStr);
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, ErrorStr);
		return false;
	}

	// We save the information for this call even if it won't be automatically processed
	FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);
	const bool bIsReadFriendsListOngoing = !LocalUser.CachedReadUserListInfo.IsEmpty();
	LocalUser.CachedReadUserListInfo.Add(ReadUserListInfo(LocalUserNum, ListName, Delegate));

	if (bIsReadFriendsListOngoing)
	{
		UE_LOG_ONLINE_FRIEND(Verbose, TEXT("A ReadFriendsList() operation for user (%d) is already running, we'll save its information and launch it automatically later."), LocalUserNum);
		return true;
	}

	EOS_Friends_QueryFriendsOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_QUERYFRIENDS_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);

	FReadFriendsCallback* CallbackObj = new FReadFriendsCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, LocalUserNum, ListName, Delegate](const EOS_Friends_QueryFriendsCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
		{
			// Handle the user logging out while a read is in progress
			Result = EOS_EResult::EOS_InvalidUser;
		}

		bool bWasSuccessful = Result == EOS_EResult::EOS_Success;
		if (bWasSuccessful)
		{
			EOS_Friends_GetFriendsCountOptions Options = { };
			Options.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST, 1);
			Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
			int32 FriendCount = EOS_Friends_GetFriendsCount(EOSSubsystem->FriendsHandle, &Options);

			TArray<EOS_EpicAccountId> FriendEasIds;
			FriendEasIds.Reserve(FriendCount);
			// Process each friend returned
			for (int32 Index = 0; Index < FriendCount; Index++)
			{
				EOS_Friends_GetFriendAtIndexOptions FriendIndexOptions = { };
				FriendIndexOptions.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST, 1);
				FriendIndexOptions.Index = Index;
				FriendIndexOptions.LocalUserId = Options.LocalUserId;
				EOS_EpicAccountId FriendEpicAccountId = EOS_Friends_GetFriendAtIndex(EOSSubsystem->FriendsHandle, &FriendIndexOptions);
				if (EOS_EpicAccountId_IsValid(FriendEpicAccountId) == EOS_TRUE)
				{
					FriendEasIds.Add(FriendEpicAccountId);
				}
			}

			const FRemoteUsersProcessedCallback Callback = [this, LocalUserNum, FriendCount](bool bWasSuccessful, TArray<FUniqueNetIdEOSRef> RemoteUserNetIds, const FString& ErrorStr)
			{
				if (bWasSuccessful)
				{
					GetLocalUserChecked(LocalUserNum).FriendsList->Empty(FriendCount);

					for (const FUniqueNetIdEOSRef& RemoteUserNetId : RemoteUserNetIds)
					{
						AddFriend(LocalUserNum, *RemoteUserNetId);
					}
				}

				// We trigger all the corresponding delegates
				ProcessReadFriendsListComplete(LocalUserNum, bWasSuccessful, ErrorStr);
			};

			AddRemotePlayers(LocalUserNum, FriendEasIds, Callback);
		}
		else
		{
			const FString ErrorString = FString::Printf(TEXT("ReadFriendsList(%d) failed with EOS result code (%s)"), LocalUserNum, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			ProcessReadFriendsListComplete(LocalUserNum, false, ErrorString);
		}
	};

	EOS_Friends_QueryFriends(EOSSubsystem->FriendsHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

	return true;
}

bool FUserManagerEOS::IsFriendQueryUserInfoOngoing(int32 LocalUserNum)
{
	const FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);

	// If we have an entry for this user and the corresponding array has any element, users are still being processed
	if (LocalUser.OngoingQueryUserInfoAccounts.Num() > 0)
	{
		return true;
	}

	if (LocalUser.OngoingPlayerQueryExternalMappings.Num() > 0)
	{
		return true;
	}

	return false;
}

void FUserManagerEOS::ProcessReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ErrorStr)
{
	// If we started any user info queries for friends, we'll just wait until they finish
	if (!IsFriendQueryUserInfoOngoing(LocalUserNum))
	{
		FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);

		// If not, we'll just trigger the delegates for all cached calls
		for (const ReadUserListInfo& CachedInfo : LocalUsers[LocalUserNum].CachedReadUserListInfo)
		{
			CachedInfo.ExecuteDelegateIfBound(bWasSuccessful, ErrorStr);
		}
		LocalUser.CachedReadUserListInfo.Empty();

		TriggerOnFriendsChangeDelegates(LocalUserNum);
	}
}

void FUserManagerEOS::SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::SetFriendAlias] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([LocalUserNum, FriendId = FriendId.AsShared(), ListName, Delegate]()
		{
			Delegate.ExecuteIfBound(LocalUserNum, *FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
		});
}

void FUserManagerEOS::DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::DeleteFriendAlias] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([LocalUserNum, FriendId = FriendId.AsShared(), ListName, Delegate]()
		{
			Delegate.ExecuteIfBound(LocalUserNum, *FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
		});
}

bool FUserManagerEOS::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::DeleteFriendsList] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([LocalUserNum, ListName, Delegate]()
		{
			Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("This method is not supported."));
		});

	return true;
}

typedef TEOSCallback<EOS_Friends_OnSendInviteCallback, EOS_Friends_SendInviteCallbackInfo, FUserManagerEOS> FSendInviteCallback;

bool FUserManagerEOS::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
	if (!LocalUsers.IsValidIndex(LocalUserNum))
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't SendInvite() for user (%d) since they are not logged in"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("Can't SendInvite() for user (%d) since they are not logged in"), LocalUserNum));
		return false;
	}

	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(FriendId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't SendInvite() for user (%d) since the potential player id is unknown"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("Can't SendInvite() for user (%d) since the player id is unknown"), LocalUserNum));
		return false;
	}

	FSendInviteCallback* CallbackObj = new FSendInviteCallback(AsWeak());
	CallbackObj->CallbackLambda = [LocalUserNum, ListName, this, Delegate](const EOS_Friends_SendInviteCallbackInfo* Data)
	{
		const FUniqueNetIdEOSRef NetIdRef = FUniqueNetIdEOSRegistry::FindChecked(Data->TargetUserId);

		FString ErrorString;
		bool bWasSuccessful = Data->ResultCode == EOS_EResult::EOS_Success;
		if (!bWasSuccessful)
		{
			ErrorString = FString::Printf(TEXT("Failed to send invite for user (%d) to player (%s) with result code (%s)"), LocalUserNum, *NetIdRef->ToString(), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
		}

		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, *NetIdRef, ListName, ErrorString);
	};

	EOS_Friends_SendInviteOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_SENDINVITE_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = AccountId;
	EOS_Friends_SendInvite(EOSSubsystem->FriendsHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

	return true;
}

typedef TEOSCallback<EOS_Friends_OnAcceptInviteCallback, EOS_Friends_AcceptInviteCallbackInfo, FUserManagerEOS> FAcceptInviteCallback;

bool FUserManagerEOS::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
	if (!LocalUsers.IsValidIndex(LocalUserNum))
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't AcceptInvite() for user (%d) since they are not logged in"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("Can't AcceptInvite() for user (%d) since they are not logged in"), LocalUserNum));
		return false;
	}

	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(FriendId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't AcceptInvite() for user (%d) since the friend is not in their list"), LocalUserNum);
		Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("Can't AcceptInvite() for user (%d) since the friend is not in their list"), LocalUserNum));
		return false;
	}

	FAcceptInviteCallback* CallbackObj = new FAcceptInviteCallback(AsWeak());
	CallbackObj->CallbackLambda = [LocalUserNum, ListName, this, Delegate](const EOS_Friends_AcceptInviteCallbackInfo* Data)
	{
		const FUniqueNetIdEOSRef NetIdRef = FUniqueNetIdEOSRegistry::FindChecked(Data->TargetUserId);

		FString ErrorString;
		bool bWasSuccessful = Data->ResultCode == EOS_EResult::EOS_Success;
		if (!bWasSuccessful)
		{
			ErrorString = FString::Printf(TEXT("Failed to accept invite for user (%d) from friend (%s) with result code (%s)"), LocalUserNum, *NetIdRef->ToString(), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
		}
		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, *NetIdRef, ListName, ErrorString);
	};

	EOS_Friends_AcceptInviteOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_ACCEPTINVITE_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = AccountId;
	EOS_Friends_AcceptInvite(EOSSubsystem->FriendsHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	return true;
}

void EOS_CALL EOSRejectInviteCallback(const EOS_Friends_RejectInviteCallbackInfo* Data)
{
	// We don't need to notify anyone so ignore
}

bool FUserManagerEOS::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (!LocalUsers.IsValidIndex(LocalUserNum))
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't RejectInvite() for user (%d) since they are not logged in"), LocalUserNum);
		return false;
	}

	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(FriendId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Can't RejectInvite() for user (%d) since the friend is not in their list"), LocalUserNum);
		return false;
	}

	EOS_Friends_RejectInviteOptions Options{ 0 };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_REJECTINVITE_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = AccountId;
	EOS_Friends_RejectInvite(EOSSubsystem->FriendsHandle, &Options, nullptr, &EOSRejectInviteCallback);
	return true;
}

bool FUserManagerEOS::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::DeleteFriend] Friends may only be deleted via the Epic Games Launcher."));

	EOSSubsystem->ExecuteNextTick([this, WeakThis = AsWeak(), LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, *FriendId, ListName, TEXT("[FUserManagerEOS::DeleteFriend] Friends may only be deleted via the Epic Games Launcher."));
			}
		});

	return true;
}

bool FUserManagerEOS::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends)
{
	OutFriends.Reset();

	for (FOnlineFriendEOSRef Friend : GetLocalUserChecked(LocalUserNum).FriendsList->GetList())
	{
		const FOnlineUserPresence& Presence = Friend->GetPresence();
		// See if they only want online only
		if (ListName == EFriendsLists::ToString(EFriendsLists::OnlinePlayers) && !Presence.bIsOnline)
		{
			continue;
		}
		// Of if they only want friends playing this game
		else if (ListName == EFriendsLists::ToString(EFriendsLists::InGamePlayers) && !Presence.bIsPlayingThisGame)
		{
			continue;
		}
		// If the service hasn't returned the info yet, skip them
		else if (Friend->GetDisplayName().IsEmpty())
		{
			continue;
		}
		OutFriends.Add(Friend);
	}

	// Sort these by those playing the game first, alphabetically, then not playing, then not online
	OutFriends.Sort([](TSharedRef<FOnlineFriend> A, TSharedRef<FOnlineFriend> B)
	{
		const FOnlineUserPresence& APres = A->GetPresence();
		const FOnlineUserPresence& BPres = B->GetPresence();
		// If they are the same, then check playing this game
		if (APres.bIsOnline == BPres.bIsOnline)
		{
			// If they are the same, then sort by name
			if (APres.bIsPlayingThisGame == BPres.bIsPlayingThisGame)
			{
				const EInviteStatus::Type AFriendStatus = A->GetInviteStatus();
				const EInviteStatus::Type BFriendStatus = B->GetInviteStatus();
				// Sort pending friends below accepted friends
				if (AFriendStatus == BFriendStatus && AFriendStatus == EInviteStatus::Accepted)
				{
					const FString& AName = A->GetDisplayName();
					const FString& BName = B->GetDisplayName();
					return AName < BName;
				}
			}
		}
		return false;
	});

	return true;
}

TSharedPtr<FOnlineFriend> FUserManagerEOS::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	const FUniqueNetIdEOS& EosId = FUniqueNetIdEOS::Cast(FriendId);
	FOnlineFriendEOSPtr FoundFriend = GetLocalUserChecked(LocalUserNum).FriendsList->GetByNetId(EosId.AsShared());
	if (FoundFriend.IsValid())
	{
		const FOnlineUserPresence& Presence = FoundFriend->GetPresence();
		// See if they only want online only
		if (ListName == EFriendsLists::ToString(EFriendsLists::OnlinePlayers) && !Presence.bIsOnline)
		{
			return TSharedPtr<FOnlineFriend>();
		}
		// Of if they only want friends playing this game
		else if (ListName == EFriendsLists::ToString(EFriendsLists::InGamePlayers) && !Presence.bIsPlayingThisGame)
		{
			return TSharedPtr<FOnlineFriend>();
		}

		return FoundFriend;
	}

	return TSharedPtr<FOnlineFriend>();
}

bool FUserManagerEOS::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	return GetFriend(LocalUserNum, FriendId, ListName).IsValid();
}

bool FUserManagerEOS::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::QueryRecentPlayers] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([this, WeakThis = AsWeak(), UserId = UserId.AsShared(), Namespace]()
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, false, TEXT("This method is not supported."));
			}
		});

	return true;
}

bool FUserManagerEOS::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::GetRecentPlayers] This method is not supported."));

	return false;
}

bool FUserManagerEOS::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::BlockPlayer] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([this, WeakThis = AsWeak(), LocalUserNum, PlayerId = PlayerId.AsShared()]()
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT(""), TEXT("This method is not supported"));
			}
		});

	return true;
}

bool FUserManagerEOS::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::UnblockPlayer] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([this, WeakThis = AsWeak(), LocalUserNum, PlayerId = PlayerId.AsShared()]()
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT(""), TEXT("This method is not supported"));
			}
		});

	return true;
}

bool FUserManagerEOS::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::QueryBlockedPlayers] This method is not supported."));

	EOSSubsystem->ExecuteNextTick([this, WeakThis = AsWeak(), UserId = UserId.AsShared()]()
		{
			if (FUserManagerEOSPtr StrongThis = WeakThis.Pin())
			{
				TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, false, TEXT("This method is not supported"));
			}
		});

	return true;
}

bool FUserManagerEOS::GetBlockedPlayers(const FUniqueNetId& UserId, TArray<TSharedRef<FOnlineBlockedPlayer>>& OutBlockedPlayers)
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::GetBlockedPlayers] This method is not supported."));

	return false;
}

void FUserManagerEOS::DumpBlockedPlayers() const
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::DumpBlockedPlayers] This method is not supported."));
}

void FUserManagerEOS::DumpRecentPlayers() const
{
	UE_LOG_ONLINE_FRIEND(Warning, TEXT("[FUserManagerEOS::DumpRecentPlayers] This method is not supported."));
}

bool FUserManagerEOS::HandleFriendsExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !UE_BUILD_SHIPPING

	bool bWasHandled = true;
	if (FParse::Command(&Cmd, TEXT("ReadFriendsList"))) /* ONLINE (EOS if using EOSPlus) FRIENDS ReadFriendsList 0 default/onlinePlayers/inGamePlayers/inGameAndSessionPlayers */
	{
		int LocalUserNum = FCString::Atoi(*FParse::Token(Cmd, false));

		FString FriendsList = FParse::Token(Cmd, false);

		ReadFriendsList(LocalUserNum, FriendsList, FOnReadFriendsListComplete());
	}
	else if (FParse::Command(&Cmd, TEXT("GetFriendsList"))) /* ONLINE (EOS if using EOSPlus) FRIENDS GetFriendsList 0 default/onlinePlayers/inGamePlayers/inGameAndSessionPlayers */
	{
		int LocalUserNum = FCString::Atoi(*FParse::Token(Cmd, false));

		FString FriendsList = FParse::Token(Cmd, false);

		TArray< TSharedRef<FOnlineFriend> > Friends;
		// Grab the friends data so we can print it out
		if (GetFriendsList(LocalUserNum, FriendsList, Friends))
		{
			UE_LOG_ONLINE_FRIEND(Log, TEXT("FUserManagerEOS::GetFriendsList returned %d friends"), Friends.Num());

			// Log each friend's data out
			for (int32 Index = 0; Index < Friends.Num(); Index++)
			{
				const FOnlineFriend& Friend = *Friends[Index];
				const FOnlineUserPresence& Presence = Friend.GetPresence();
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t%s has unique id (%s)"), *Friend.GetDisplayName(), *Friend.GetUserId()->ToDebugString());
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t Invite status (%s)"), EInviteStatus::ToString(Friend.GetInviteStatus()));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t Presence: %s"), *Presence.Status.StatusStr);
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t State: %s"), EOnlinePresenceState::ToString(Presence.Status.State));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsOnline (%s)"), Presence.bIsOnline ? TEXT("true") : TEXT("false"));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsPlaying (%s)"), Presence.bIsPlaying ? TEXT("true") : TEXT("false"));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsPlayingThisGame (%s)"), Presence.bIsPlayingThisGame ? TEXT("true") : TEXT("false"));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsJoinable (%s)"), Presence.bIsJoinable ? TEXT("true") : TEXT("false"));
				UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bHasVoiceSupport (%s)"), Presence.bHasVoiceSupport ? TEXT("true") : TEXT("false"));
			}
		}
	}
	else if (FParse::Command(&Cmd, TEXT("GetFriend"))) /* ONLINE (EOS if using EOSPlus) FRIENDS GetFriend 0 "FriendUserId|FullStr" default/onlinePlayers/inGamePlayers/inGameAndSessionPlayers */
	{
		int LocalUserNum = FCString::Atoi(*FParse::Token(Cmd, false));

		FString FriendUserIdStr = FParse::Token(Cmd, false);
		FUniqueNetIdEOSRef FriendEosId = FUniqueNetIdEOSRegistry::FindOrAdd(FriendUserIdStr);

		FString FriendsList = FParse::Token(Cmd, false);

		TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum, *FriendEosId, FriendsList);
		if (Friend.IsValid())
		{
			const FOnlineUserPresence& Presence = Friend->GetPresence();
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t%s has unique id (%s)"), *Friend->GetDisplayName(), *Friend->GetUserId()->ToDebugString());
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t Invite status (%s)"), EInviteStatus::ToString(Friend->GetInviteStatus()));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t Presence: %s"), *Presence.Status.StatusStr);
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t State: %s"), EOnlinePresenceState::ToString(Presence.Status.State));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsOnline (%s)"), Presence.bIsOnline ? TEXT("true") : TEXT("false"));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsPlaying (%s)"), Presence.bIsPlaying ? TEXT("true") : TEXT("false"));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsPlayingThisGame (%s)"), Presence.bIsPlayingThisGame ? TEXT("true") : TEXT("false"));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bIsJoinable (%s)"), Presence.bIsJoinable ? TEXT("true") : TEXT("false"));
			UE_LOG_ONLINE_FRIEND(Log, TEXT("\t\t bHasVoiceSupport (%s)"), Presence.bHasVoiceSupport ? TEXT("true") : TEXT("false"));
		}
	}
	else if (FParse::Command(&Cmd, TEXT("IsFriend"))) /* ONLINE (EOS if using EOSPlus) FRIENDS IsFriend 0 "FriendUserId|FullStr" default/onlinePlayers/inGamePlayers/inGameAndSessionPlayers */
	{
		int LocalUserNum = FCString::Atoi(*FParse::Token(Cmd, false));

		FString FriendUserIdStr = FParse::Token(Cmd, false);
		FUniqueNetIdEOSRef FriendEosId = FUniqueNetIdEOSRegistry::FindOrAdd(FriendUserIdStr);

		FString FriendsList = FParse::Token(Cmd, false);

		bool bIsFriend = IsFriend(LocalUserNum, *FriendEosId, FriendsList);
		UE_LOG_ONLINE_FRIEND(Log, TEXT("UserId=%s bIsFriend=%s"), *FriendUserIdStr, *LexToString(bIsFriend));
	}
	else
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("Unknown FRIENDS command: %s"), *FParse::Token(Cmd, true));
		bWasHandled = false;
	}

	return bWasHandled;
#else
	return false;
#endif // !UE_BUILD_SHIPPING
}

// ~IOnlineFriends Interface

struct FPresenceStrings
{
	FPresenceStrings(const FString& InKey, const FString& InValue)
		: Key(*InKey), Value(*InValue)
	{
	}
	FTCHARToUTF8 Key;
	FTCHARToUTF8 Value;
};

struct FRichTextOptions :
	public EOS_PresenceModification_SetRawRichTextOptions
{
	FRichTextOptions() :
		EOS_PresenceModification_SetRawRichTextOptions()
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_SETRAWRICHTEXT_API_LATEST, 1);
		RichText = RichTextAnsi;
	}
	char RichTextAnsi[EOS_PRESENCE_RICH_TEXT_MAX_VALUE_LENGTH];
};

typedef TEOSCallback<EOS_Presence_SetPresenceCompleteCallback, EOS_Presence_SetPresenceCallbackInfo, FUserManagerEOS> FSetPresenceCallback;

void FUserManagerEOS::SetPresence(const FUniqueNetId& UserId, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(UserId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE(Error, TEXT("Can't SetPresence() for user (%s) since they are not logged in"), *EOSID.ToDebugString());
		return;
	}

	EOS_HPresenceModification ChangeHandle = nullptr;
	EOS_Presence_CreatePresenceModificationOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST, 1);
	Options.LocalUserId = AccountId;
	EOS_Presence_CreatePresenceModification(EOSSubsystem->PresenceHandle, &Options, &ChangeHandle);
	if (ChangeHandle == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to create a modification handle for setting presence"));
		return;
	}

	EOS_PresenceModification_SetStatusOptions StatusOptions = { };
	StatusOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_SETSTATUS_API_LATEST, 1);
	StatusOptions.Status = ToEOS_Presence_EStatus(Status.State);
	EOS_EResult SetStatusResult = EOS_PresenceModification_SetStatus(ChangeHandle, &StatusOptions);
	if (SetStatusResult != EOS_EResult::EOS_Success)
	{
		UE_LOG_ONLINE(Error, TEXT("EOS_PresenceModification_SetStatus() failed with result code (%d)"), (int32)SetStatusResult);
	}

	// Convert the status string as the rich text string
	FRichTextOptions TextOptions;
	FCStringAnsi::Strncpy(TextOptions.RichTextAnsi, TCHAR_TO_UTF8(*Status.StatusStr), EOS_PRESENCE_RICH_TEXT_MAX_VALUE_LENGTH);
	EOS_EResult SetRichTextResult = EOS_PresenceModification_SetRawRichText(ChangeHandle, &TextOptions);
	if (SetRichTextResult != EOS_EResult::EOS_Success)
	{
		UE_LOG_ONLINE(Error, TEXT("EOS_PresenceModification_SetRawRichText() failed with result code (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(SetRichTextResult)));
	}

	TArray<FPresenceStrings, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> RawStrings;
	TArray<EOS_Presence_DataRecord, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> Records;
	int32 CurrentIndex = 0;
	// Loop through the properties building records
	for (FPresenceProperties::TConstIterator It(Status.Properties); It && CurrentIndex < EOS_PRESENCE_DATA_MAX_KEYS; ++It, ++CurrentIndex)
	{
		const FPresenceStrings& RawString = RawStrings.Emplace_GetRef(It.Key(), It.Value().ToString());

		EOS_Presence_DataRecord& Record = Records.Emplace_GetRef();
		Record.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_DATARECORD_API_LATEST, 1);
		Record.Key = RawString.Key.Get();
		Record.Value = RawString.Value.Get();
	}

	if (Records.Num() > 0)
	{
		EOS_PresenceModification_SetDataOptions DataOptions = { };
		DataOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_SETDATA_API_LATEST, 1);
		DataOptions.RecordsCount = Records.Num();
		DataOptions.Records = Records.GetData();
		EOS_EResult SetDataResult = EOS_PresenceModification_SetData(ChangeHandle, &DataOptions);
		if (SetDataResult != EOS_EResult::EOS_Success)
		{
			UE_LOG_ONLINE(Error, TEXT("EOS_PresenceModification_SetData() failed with result code (%s)"), *LexToString(SetDataResult));
		}
	}

	FSetPresenceCallback* CallbackObj = new FSetPresenceCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, Delegate](const EOS_Presence_SetPresenceCallbackInfo* Data)
	{
		const bool bSuccess = Data->ResultCode == EOS_EResult::EOS_Success;
		UE_CLOG_ONLINE(!bSuccess, Error, TEXT("SetPresence() failed with result code (%s)"), *LexToString(Data->ResultCode));
		Delegate.ExecuteIfBound(*GetLocalUniqueNetIdEOS(Data->LocalUserId), bSuccess);
	};

	EOS_Presence_SetPresenceOptions PresOptions = { };
	PresOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_SETPRESENCE_API_LATEST, 1);
	PresOptions.LocalUserId = AccountId;
	PresOptions.PresenceModificationHandle = ChangeHandle;
	// Last step commit the changes
	EOS_Presence_SetPresence(EOSSubsystem->PresenceHandle, &PresOptions, CallbackObj, CallbackObj->GetCallbackPtr());
	EOS_PresenceModification_Release(ChangeHandle);
}

typedef TEOSCallback<EOS_Presence_OnQueryPresenceCompleteCallback, EOS_Presence_QueryPresenceCallbackInfo, FUserManagerEOS> FQueryPresenceCallback;

void FUserManagerEOS::QueryPresence(const FUniqueNetId& UserId, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if (DefaultLocalUser < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Can't QueryPresence() due to no users being signed in"));
		Delegate.ExecuteIfBound(UserId, false);
		return;
	}

	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(UserId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE(Error, TEXT("Can't QueryPresence(%s) for unknown unique net id"), *EOSID.ToDebugString());
		Delegate.ExecuteIfBound(UserId, false);
		return;
	}

	EOS_Presence_HasPresenceOptions HasOptions = { };
	HasOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_HASPRESENCE_API_LATEST, 1);
	HasOptions.LocalUserId = GetLocalEpicAccountId(DefaultLocalUser);
	HasOptions.TargetUserId = AccountId;
	EOS_Bool bHasPresence = EOS_Presence_HasPresence(EOSSubsystem->PresenceHandle, &HasOptions);
	if (bHasPresence == EOS_FALSE)
	{
		FQueryPresenceCallback* CallbackObj = new FQueryPresenceCallback(AsWeak());
		CallbackObj->CallbackLambda = [this, Delegate](const EOS_Presence_QueryPresenceCallbackInfo* Data)
		{
			// If we were able to retrieve a UniqueNetId to pass to this method, it must already be registered
			const FUniqueNetIdEOSRef NetIdPtr = FUniqueNetIdEOSRegistry::FindChecked(Data->TargetUserId);

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				// Update the presence data to the most recent
				UpdatePresence(DefaultLocalUser, Data->TargetUserId);
				Delegate.ExecuteIfBound(*NetIdPtr, true);
			}
			else
			{
				UE_LOG_ONLINE(Error, TEXT("QueryPresence() for user (%s) failed with result code (%s)"), *NetIdPtr->ToString(), *LexToString(Data->ResultCode));
				Delegate.ExecuteIfBound(*FUniqueNetIdEOS::EmptyId(), false);
			}
		};

		// Query for updated presence
		EOS_Presence_QueryPresenceOptions Options = { };
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_QUERYPRESENCE_API_LATEST, 1);
		Options.LocalUserId = HasOptions.LocalUserId;
		Options.TargetUserId = HasOptions.TargetUserId;
		EOS_Presence_QueryPresence(EOSSubsystem->PresenceHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
		return;
	}

	// Update the presence data to the most recent
	UpdatePresence(DefaultLocalUser, HasOptions.TargetUserId);
	// It's already present so trigger that it's done
	Delegate.ExecuteIfBound(UserId, true);
}

void FUserManagerEOS::UpdatePresence(int32 LocalUserNum, EOS_EpicAccountId AccountId)
{
	EOS_Presence_Info* PresenceInfo = nullptr;
	EOS_Presence_CopyPresenceOptions Options = { };
	Options.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_COPYPRESENCE_API_LATEST, 3);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = AccountId;
	EOS_EResult CopyResult = EOS_Presence_CopyPresence(EOSSubsystem->PresenceHandle, &Options, &PresenceInfo);
	if (CopyResult == EOS_EResult::EOS_Success)
	{
		const FUniqueNetIdEOSRef& NetId = FUniqueNetIdEOSRegistry::FindChecked(AccountId);

		// Create it on demand if we don't have one yet
		if (!UniqueNetIdToOnlineUserPresenceMap.Contains(NetId))
		{
			FOnlineUserPresenceRef PresenceRef = MakeShareable(new FOnlineUserPresence());
			UniqueNetIdToOnlineUserPresenceMap.Emplace(NetId, PresenceRef);
		}

		FOnlineUserPresenceRef PresenceRef = UniqueNetIdToOnlineUserPresenceMap[NetId];
		const FString ProductId(UTF8_TO_TCHAR(PresenceInfo->ProductId));
		const FString ProdVersion(UTF8_TO_TCHAR(PresenceInfo->ProductVersion));
		const FString Platform(UTF8_TO_TCHAR(PresenceInfo->Platform));
		// Convert the presence data to our format
		PresenceRef->Status.State = ToEOnlinePresenceState(PresenceInfo->Status);
		PresenceRef->Status.StatusStr = UTF8_TO_TCHAR(PresenceInfo->RichText);
		PresenceRef->bIsOnline = PresenceRef->Status.State == EOnlinePresenceState::Online;
		PresenceRef->bIsPlaying = !ProductId.IsEmpty();
		PresenceRef->bIsPlayingThisGame = ProductId == EOSSubsystem->ProductId && ProdVersion == EOSSubsystem->EOSSDKManager->GetProductVersion();
//		PresenceRef->bIsJoinable = ???;
//		PresenceRef->bHasVoiceSupport = ???;
		PresenceRef->Status.Properties.Add(TEXT("ProductId"), ProductId);
		PresenceRef->Status.Properties.Add(TEXT("ProductVersion"), ProdVersion);
		PresenceRef->Status.Properties.Add(TEXT("Platform"), Platform);
		for (int32 Index = 0; Index < PresenceInfo->RecordsCount; Index++)
		{
			const EOS_Presence_DataRecord& Record = PresenceInfo->Records[Index];
			PresenceRef->Status.Properties.Add(Record.Key, UTF8_TO_TCHAR(Record.Value));
		}

		// Copy the presence if this is a friend that was updated, so that their data is in sync
		UpdateFriendPresence(NetId, PresenceRef);
		
		TriggerOnPresenceReceivedDelegates(*NetId, PresenceRef);

		EOS_Presence_Info_Release(PresenceInfo);
	}
	else
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to copy presence data with error code (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(CopyResult)));
	}
}

void FUserManagerEOS::UpdateFriendPresence(const FUniqueNetIdEOSRef& FriendId, FOnlineUserPresenceRef Presence)
{
	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			const FLocalUserEOS& LocalUser = LocalUsers[Index];
			
			if (FFriendsListEOSPtr FriendsList = LocalUser.FriendsList)
			{
				FOnlineFriendEOSPtr Friend = FriendsList->GetByNetId(FriendId);
				if (Friend.IsValid())
				{
					Friend->SetPresence(Presence);
				}
			}
		}
	}
}

EOnlineCachedResult::Type FUserManagerEOS::GetCachedPresence(const FUniqueNetId& UserId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(UserId);
	if (UniqueNetIdToOnlineUserPresenceMap.Contains(EOSID.AsShared()))
	{
		OutPresence = UniqueNetIdToOnlineUserPresenceMap[EOSID.AsShared()];
		return EOnlineCachedResult::Success;
	}
	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FUserManagerEOS::GetCachedPresenceForApp(const FUniqueNetId&, const FUniqueNetId& UserId, const FString&, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	return GetCachedPresence(UserId, OutPresence);
}

bool FUserManagerEOS::QueryUserInfo(int32 LocalUserNum, const TArray<FUniqueNetIdRef>& UserIds)
{
	const FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);
	if (!LocalUser.OngoingQueryUserInfoAccounts.IsEmpty())
	{
		UE_LOG_ONLINE(Verbose, TEXT("[FUserManagerEOS::QueryUserInfo] User Info query already ongoing for local user %d"), LocalUserNum);
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, TArray<FUniqueNetIdRef>(), FString());
	}

	TArray<FString> UserEasIdsNeedingExternalMappings;
	UserEasIdsNeedingExternalMappings.Reserve(UserIds.Num());
	
	// Trigger a query for each user in the list
	for (const FUniqueNetIdRef& NetId : UserIds)
	{
		const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(*NetId);

		// Skip querying for local users since we already have that data
		if (IsLocalUser(EOSID))
		{
			continue;
		}

		// Check to see if we know about this user or not
		const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
		if (EOS_EpicAccountId_IsValid(AccountId) == EOS_TRUE)
		{
			const FRemoteUserProcessedCallback Callback = [this, LocalUserNum](bool bWasSuccessful, FUniqueNetIdEOSRef RemoteUserNetId, const FString& ErrorStr)
			{
				static TArray<FUniqueNetIdRef> ProcessedIds;
				ProcessedIds.Add(RemoteUserNetId);

				if(GetLocalUserChecked(LocalUserNum).OngoingQueryUserInfoAccounts.IsEmpty())
				{
					TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bWasSuccessful, ProcessedIds, ErrorStr);
					
					ProcessedIds.Empty();
				}
			};

			// If the user is already registered, we'll update their user info
			if (UniqueNetIdToUserRefMap.Contains(EOSID.AsShared()))
			{
				ReadUserInfo(LocalUserNum, AccountId, Callback);
			}
			else
			{
				// If the user is not registered, we'll add it and query their user info
				UserEasIdsNeedingExternalMappings.Add(LexToString(AccountId));

				// Registering the player will also query the user info data
				AddRemotePlayer(LocalUserNum, AccountId, Callback);
			}
		}
	}
	
	return true;
}

typedef TEOSCallback<EOS_UserInfo_OnQueryUserInfoCallback, EOS_UserInfo_QueryUserInfoCallbackInfo, FUserManagerEOS> FReadUserInfoCallback;

void FUserManagerEOS::ReadUserInfo(int32 LocalUserNum, EOS_EpicAccountId EpicAccountId, const FRemoteUserProcessedCallback& Callback)
{
	FReadUserInfoCallback* CallbackObj = new FReadUserInfoCallback(AsWeak());
	CallbackObj->CallbackLambda = [this, LocalUserNum, EpicAccountId, Callback](const EOS_UserInfo_QueryUserInfoCallbackInfo* Data)
	{
		const FUniqueNetIdEOSRef EOSId = FUniqueNetIdEOSRegistry::FindChecked(Data->TargetUserId);

		const bool bWasSuccessful = Data->ResultCode == EOS_EResult::EOS_Success;
		FString ErrorStr;

		if (bWasSuccessful)
		{
			IAttributeAccessInterfaceRef AttributeAccessRef = UniqueNetIdToUserRefMap[EOSId];
			UpdateUserInfo(AttributeAccessRef, Data->LocalUserId, Data->TargetUserId);

			FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);
			if (FOnlineFriendEOSPtr FriendPtr = LocalUser.FriendsList->GetByNetId(EOSId))
			{
				FriendPtr->UpdateInternalAttributes(AttributeAccessRef->GetInternalAttributes());
			}
		}
		else
		{
			ErrorStr = LexToString(Data->ResultCode);
		}

		// We mark this player as processed
		GetLocalUserChecked(LocalUserNum).OngoingQueryUserInfoAccounts.RemoveSwap(EpicAccountId, EAllowShrinking::No);

		Callback(bWasSuccessful, EOSId, ErrorStr);
	};

	EOS_UserInfo_QueryUserInfoOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_QUERYUSERINFO_API_LATEST, 1);
	Options.LocalUserId = GetLocalEpicAccountId(LocalUserNum);
	Options.TargetUserId = EpicAccountId;
	EOS_UserInfo_QueryUserInfo(EOSSubsystem->UserInfoHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

	// We mark this player as pending for processing
	GetLocalUserChecked(LocalUserNum).OngoingQueryUserInfoAccounts.Add(EpicAccountId);
}

bool FUserManagerEOS::GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser>>& OutUsers)
{
	OutUsers.Reset();

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			if (const FUserOnlineAccountEOSPtr UserOnlineAccount = LocalUsers[Index].UserOnlineAccount)
			{
				OutUsers.Add(UserOnlineAccount.ToSharedRef());
			}
		}
	}

	return true;
}

TSharedPtr<FOnlineUser> FUserManagerEOS::GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId)
{
	TSharedPtr<FOnlineUser> OnlineUser;

	for (int32 Index = 0; Index < LocalUsers.GetMaxIndex(); Index++)
	{
		if (LocalUsers.IsValidIndex(Index))
		{
			const FLocalUserEOS& LocalUser = LocalUsers[Index];

			if (*LocalUser.UniqueNetId == UserId && LocalUser.UserOnlineAccount.IsValid())
			{
				OnlineUser = LocalUser.UserOnlineAccount;
			}
		}
	}

	return OnlineUser;
}

struct FQueryByDisplayNameOptions :
	public EOS_UserInfo_QueryUserInfoByDisplayNameOptions
{
	FQueryByDisplayNameOptions() :
		EOS_UserInfo_QueryUserInfoByDisplayNameOptions()
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST, 1);
		DisplayName = DisplayNameAnsi;
	}
	char DisplayNameAnsi[EOS_OSS_STRING_BUFFER_LENGTH];
};

typedef TEOSCallback<EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback, EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo, FUserManagerEOS> FQueryInfoByNameCallback;

bool FUserManagerEOS::QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate)
{
	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(UserId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		UE_LOG_ONLINE(Error, TEXT("Specified local user (%s) is not known"), *EOSID.ToDebugString());
		Delegate.ExecuteIfBound(false, UserId, DisplayNameOrEmail, *FUniqueNetIdEOS::EmptyId(), FString::Printf(TEXT("Specified local user (%s) is not known"), *EOSID.ToDebugString()));
		return false;
	}
	int32 LocalUserNum = GetLocalUserNumFromUniqueNetId(UserId);

	FQueryInfoByNameCallback* CallbackObj = new FQueryInfoByNameCallback(AsWeak());
	CallbackObj->CallbackLambda = [LocalUserNum, DisplayNameOrEmail, this, Delegate](const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
		{
			// Handle the user logging out while a read is in progress
			Result = EOS_EResult::EOS_InvalidUser;
		}

		FString ErrorString;
		bool bWasSuccessful = Result == EOS_EResult::EOS_Success;
		if (bWasSuccessful)
		{
			FUniqueNetIdEOSPtr NetId = FUniqueNetIdEOSRegistry::Find(Data->TargetUserId);
			bool bIsNetIdRegistered = NetId.IsValid() ? UniqueNetIdToUserRefMap.Contains(NetId.ToSharedRef()) : false;
			if (!bIsNetIdRegistered)
			{
				const FRemoteUserProcessedCallback Callback = [this, LocalUserNum, DisplayNameOrEmail, Delegate](bool bWasSuccessful, FUniqueNetIdEOSRef RemotePlayerNetId, const FString& ErrorStr)
				{
					Delegate.ExecuteIfBound(bWasSuccessful, *GetLocalUserChecked(LocalUserNum).UniqueNetId, DisplayNameOrEmail, *RemotePlayerNetId, ErrorStr);
				};

				// Registering the player will also query the presence/user info data
				AddRemotePlayer(LocalUserNum, Data->TargetUserId, Callback);
			}
		}
		else
		{
			ErrorString = FString::Printf(TEXT("QueryUserIdMapping(%d, '%s') failed with EOS result code (%s)"), DefaultLocalUser, *DisplayNameOrEmail, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		}
		Delegate.ExecuteIfBound(false, *FUniqueNetIdEOS::EmptyId(), DisplayNameOrEmail, *FUniqueNetIdEOS::EmptyId(), ErrorString);
	};

	FQueryByDisplayNameOptions Options;
	FCStringAnsi::Strncpy(Options.DisplayNameAnsi, TCHAR_TO_UTF8(*DisplayNameOrEmail), EOS_OSS_STRING_BUFFER_LENGTH);
	Options.LocalUserId = AccountId;
	EOS_UserInfo_QueryUserInfoByDisplayName(EOSSubsystem->UserInfoHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

	return true;
}

struct FQueryByStringIdsOptions :
	public EOS_Connect_QueryExternalAccountMappingsOptions
{
	FQueryByStringIdsOptions(const uint32 InNumStringIds, EOS_ProductUserId InLocalUserId) :
		EOS_Connect_QueryExternalAccountMappingsOptions()
	{
		PointerArray.AddZeroed(InNumStringIds);
		for (int32 Index = 0; Index < PointerArray.Num(); Index++)
		{
			PointerArray[Index] = new char[EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH+1];
		}
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST, 1);
		AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
		ExternalAccountIds = (const char**)PointerArray.GetData();
		ExternalAccountIdCount = InNumStringIds;
		LocalUserId = InLocalUserId;
	}

	~FQueryByStringIdsOptions()
	{
		for (int32 Index = 0; Index < PointerArray.Num(); Index++)
		{
			delete [] PointerArray[Index];
		}
	}
	TArray<char*> PointerArray;
};

struct FGetAccountMappingOptions :
	public EOS_Connect_GetExternalAccountMappingsOptions
{
	FGetAccountMappingOptions() :
		EOS_Connect_GetExternalAccountMappingsOptions()
	{
		ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST, 1);
		AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
		TargetExternalUserId = AccountId;
	}
	char AccountId[EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH+1];
};

typedef TEOSCallback<EOS_Connect_OnQueryExternalAccountMappingsCallback, EOS_Connect_QueryExternalAccountMappingsCallbackInfo, FUserManagerEOS> FQueryByStringIdsCallback;

bool FUserManagerEOS::QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate)
{
	const FUniqueNetIdEOS& EOSID = FUniqueNetIdEOS::Cast(UserId);
	const EOS_EpicAccountId AccountId = EOSID.GetEpicAccountId();
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
	{
		Delegate.ExecuteIfBound(false, UserId, QueryOptions, ExternalIds, FString::Printf(TEXT("User (%s) is not logged in, so can't query external account ids"), *EOSID.ToDebugString()));
		return false;
	}

	if (ExternalIds.IsEmpty())
	{
		Delegate.ExecuteIfBound(false, UserId, QueryOptions, ExternalIds, FString::Printf(TEXT("List of ids to query is empty for User (%s), so can't query external account ids"), *EOSID.ToDebugString()));
		return false;
	}

	int32 LocalUserNum = GetLocalUserNumFromUniqueNetId(UserId);

	// Mark the queries as in progress
	GetLocalUserChecked(LocalUserNum).OngoingPlayerQueryExternalMappings.Append(ExternalIds);

	const EOS_ProductUserId LocalUserId = EOSID.GetProductUserId();
	const uint32 MaxBatchSize = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_MAX_ACCOUNT_IDS;
	const uint32 NumBatches = FMath::DivideAndRoundUp<uint32>(ExternalIds.Num(), MaxBatchSize);
	// Process queries in batches since there's a max that can be done at once
	for (uint32 BatchIdx = 0; BatchIdx < NumBatches; BatchIdx++)
	{
		const uint32 BatchSrcOffset = BatchIdx * MaxBatchSize;
		const uint32 BatchSize = FMath::Min(ExternalIds.Num() - BatchSrcOffset, MaxBatchSize);

		// Build an options up per batch
		FQueryByStringIdsOptions Options(BatchSize, LocalUserId);
		for (uint32 DestIdx = 0, SrcIdx = BatchSrcOffset; DestIdx < BatchSize; DestIdx++, SrcIdx++)
		{
			FCStringAnsi::Strncpy(Options.PointerArray[DestIdx], TCHAR_TO_UTF8(*ExternalIds[SrcIdx]), EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH+1);
		}

		TArray<FString> BatchIds(ExternalIds.GetData() + BatchSrcOffset, BatchSize);
		FQueryByStringIdsCallback* CallbackObj = new FQueryByStringIdsCallback(AsWeak());
		CallbackObj->CallbackLambda = [LocalUserNum, QueryOptions, BatchIds = MoveTemp(BatchIds), this, Delegate](const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
		{
			EOS_EResult Result = Data->ResultCode;
			if (GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
			{
				// Handle the user logging out while a read is in progress
				Result = EOS_EResult::EOS_InvalidUser;
			}

			FString ErrorString;
			FUniqueNetIdEOSPtr EOSID = FUniqueNetIdEOS::EmptyId();
			if (Result == EOS_EResult::EOS_Success)
			{
				EOSID = GetLocalUserChecked(LocalUserNum).UniqueNetId;

				FGetAccountMappingOptions Options;
				Options.LocalUserId = GetLocalProductUserId(LocalUserNum);
				// Get the product id for each epic account passed in
				for (const FString& StringId : BatchIds)
				{
					FCStringAnsi::Strncpy(Options.AccountId, TCHAR_TO_UTF8(*StringId), EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH + 1);
					EOS_ProductUserId ProductUserId = EOS_Connect_GetExternalAccountMapping(EOSSubsystem->ConnectHandle, &Options);
					
					// Even if the ProductUserId retrieved above is null, we'll still add it to the registry, as that just means that the EAS account does not have a linked EOS account yet
					EOS_EpicAccountId AccountId = EOS_EpicAccountId_FromString(Options.AccountId);
					FUniqueNetIdEOSRegistry::FindOrAdd(AccountId, ProductUserId);
				}
			}
			else
			{
				ErrorString = FString::Printf(TEXT("EOS_Connect_QueryExternalAccountMappings() failed with result code (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			}

			// Mark all queries as complete
			FLocalUserEOS& LocalUser = GetLocalUserChecked(LocalUserNum);
			for (const FString& StringId : BatchIds)
			{
				LocalUser.OngoingPlayerQueryExternalMappings.RemoveSwap(StringId, EAllowShrinking::No);
			}

			const bool bWasSuccessful = Result == EOS_EResult::EOS_Success;
			Delegate.ExecuteIfBound(bWasSuccessful, *EOSID, QueryOptions, BatchIds, ErrorString);
		};

		EOS_Connect_QueryExternalAccountMappings(EOSSubsystem->ConnectHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
	}
	return true;
}

void FUserManagerEOS::GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<FUniqueNetIdPtr>& OutIds)
{
	OutIds.Reset();
	for (const FString& AccountIdStr : ExternalIds)
	{
		OutIds.Add(GetExternalIdMapping(QueryOptions, AccountIdStr));
	}
}

FUniqueNetIdPtr FUserManagerEOS::GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId)
{
	FUniqueNetIdPtr NetId;
	EOS_EpicAccountId AccountId = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*ExternalId));
	if (EOS_EpicAccountId_IsValid(AccountId) == EOS_TRUE)
	{
		return FUniqueNetIdEOSRegistry::Find(AccountId);
	}
	return NetId;
}

#endif