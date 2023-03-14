// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/AuthEOSGS.h"

#include "Containers/BackgroundableTicker.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CommandLine.h"
#include "Online/AuthErrors.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineIdEOSGS.h"
#include "Online/OnlineServicesEOSGS.h"

#include "eos_auth.h"
#include "eos_common.h"
#include "eos_connect.h"
#include "eos_init.h"
#include "eos_logging.h"
#include "eos_sdk.h"
#include "eos_types.h"
#include "eos_userinfo.h"

namespace UE::Online {

struct FAuthEOSGSLoginConfig
{
	bool EASAuthEnabled = false;
	TArray<FString> DefaultScopes;
};

struct FAuthEOSGSLoginRecoveryConfig
{
	float Interval = 30.f;
	float Jitter = 5.f;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAuthEOSGSLoginConfig)
	ONLINE_STRUCT_FIELD(FAuthEOSGSLoginConfig, EASAuthEnabled),
	ONLINE_STRUCT_FIELD(FAuthEOSGSLoginConfig, DefaultScopes)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthEOSGSLoginRecoveryConfig)
	ONLINE_STRUCT_FIELD(FAuthEOSGSLoginRecoveryConfig, Interval),
	ONLINE_STRUCT_FIELD(FAuthEOSGSLoginRecoveryConfig, Jitter)
END_ONLINE_STRUCT_META()

/* Meta*/ }

namespace {

static const FString AccountInfoKeyName = TEXT("AccountInfoEOS");

/* anonymous */ }

enum class EEOSConnectTranslationFlags : uint8
{
	None = 0,
	DisplayName = 1 << 0,
};
ENUM_CLASS_FLAGS(EEOSConnectTranslationFlags);

struct FEOSConnectTranslationTraits
{
	//FEOSConnectTranslationTraits() = delete;
	EOS_EExternalCredentialType Type;
	EEOSConnectTranslationFlags Flags;
};

class FEOSConnectLoginOptions : public EOS_Connect_LoginOptions
{
public:
	FEOSConnectLoginOptions(const FEOSConnectLoginOptions&) = delete;
	FEOSConnectLoginOptions& operator=(const FEOSConnectLoginOptions&) = delete;
	FEOSConnectLoginOptions(FEOSConnectLoginOptions&&);
	FEOSConnectLoginOptions& operator=(FEOSConnectLoginOptions&&);
	virtual ~FEOSConnectLoginOptions() = default;

	static TDefaultErrorResultInternal<FEOSConnectLoginOptions> Create(FPlatformUserId PlatformUserId, const FExternalAuthToken& ExternalAuthToken);
private:
	FEOSConnectLoginOptions();

	static const FEOSConnectTranslationTraits* GetConnectTranslationTraits(FName ExternalLoginType);

	EOS_Connect_Credentials CredentialsData;
	EOS_Connect_UserLoginInfo UserLoginInfoData;
	char DisplayNameUtf8[EOS_CONNECT_USERLOGININFO_DISPLAYNAME_MAX_LENGTH + 1] = {};
	TArray<char> ExternalAuthTokenUtf8;
};

const FEOSConnectTranslationTraits* FEOSConnectLoginOptions::GetConnectTranslationTraits(FName ExternalLoginType)
{
	static const TMap<FName, FEOSConnectTranslationTraits> SupportedLoginTranslatorTraits = {
		{ ExternalLoginType::Epic, { EOS_EExternalCredentialType::EOS_ECT_EPIC, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::SteamAppTicket, { EOS_EExternalCredentialType::EOS_ECT_STEAM_APP_TICKET, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::PsnIdToken, { EOS_EExternalCredentialType::EOS_ECT_PSN_ID_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::XblXstsToken, { EOS_EExternalCredentialType::EOS_ECT_XBL_XSTS_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::DiscordAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DISCORD_ACCESS_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::GogSessionTicket, { EOS_EExternalCredentialType::EOS_ECT_GOG_SESSION_TICKET, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::NintendoIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_ID_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::NintendoNsaIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_NSA_ID_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::UplayAccessToken, { EOS_EExternalCredentialType::EOS_ECT_UPLAY_ACCESS_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::OpenIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_OPENID_ACCESS_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::DeviceIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::AppleIdToken, { EOS_EExternalCredentialType::EOS_ECT_APPLE_ID_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::GoogleIdToken, { EOS_EExternalCredentialType::EOS_ECT_GOOGLE_ID_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::OculusUserIdNonce, { EOS_EExternalCredentialType::EOS_ECT_OCULUS_USERID_NONCE, EEOSConnectTranslationFlags::DisplayName } },
		{ ExternalLoginType::ItchioJwt, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_JWT, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::ItchioKey, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_KEY, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::EpicIdToken, { EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN, EEOSConnectTranslationFlags::None } },
		{ ExternalLoginType::AmazonAccessToken, { EOS_EExternalCredentialType::EOS_ECT_AMAZON_ACCESS_TOKEN, EEOSConnectTranslationFlags::DisplayName } },
	};

	return SupportedLoginTranslatorTraits.Find(ExternalLoginType);
}

FEOSConnectLoginOptions::FEOSConnectLoginOptions(FEOSConnectLoginOptions&& Other)
{
	*this = MoveTemp(Other);
}

FEOSConnectLoginOptions& FEOSConnectLoginOptions::operator=(FEOSConnectLoginOptions&& Other)
{
	ApiVersion = Other.ApiVersion;
	Credentials = nullptr;
	UserLoginInfo = nullptr;

	if (Other.Credentials)
	{
		ExternalAuthTokenUtf8 = MoveTemp(Other.ExternalAuthTokenUtf8);
		CredentialsData = Other.CredentialsData;
		CredentialsData.Token = ExternalAuthTokenUtf8.GetData();
		Credentials = &CredentialsData;
		Other.CredentialsData = {};
	}

	if (Other.UserLoginInfo)
	{
		memcpy(DisplayNameUtf8, Other.DisplayNameUtf8, sizeof(DisplayNameUtf8));
		UserLoginInfoData = Other.UserLoginInfoData;
		UserLoginInfoData.DisplayName = DisplayNameUtf8;
		UserLoginInfo = &UserLoginInfoData;
		Other.UserLoginInfo = nullptr;
		Other.UserLoginInfoData = {};
	}

	return *this;
}

FEOSConnectLoginOptions::FEOSConnectLoginOptions()
{
	// EOS_Connect_LoginOptions init
	static_assert(EOS_CONNECT_LOGIN_API_LATEST == 2, "EOS_Connect_LoginOptions updated, check new fields");
	ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	Credentials = &CredentialsData;
	UserLoginInfo = nullptr;

	// EOS_Connect_Credentials init
	static_assert(EOS_CONNECT_CREDENTIALS_API_LATEST == 1, "EOS_Connect_Credentials updated, check new fields");
	CredentialsData.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	CredentialsData.Token = nullptr;

	// EOS_Connect_UserLoginInfo init
	static_assert(EOS_CONNECT_USERLOGININFO_API_LATEST == 1, "EOS_Connect_UserLoginInfo updated, check new fields");
	UserLoginInfoData.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
	UserLoginInfoData.DisplayName = DisplayNameUtf8;
}

TDefaultErrorResultInternal<FEOSConnectLoginOptions> FEOSConnectLoginOptions::Create(FPlatformUserId PlatformUserId, const FExternalAuthToken& ExternalAuthToken)
{
	const FEOSConnectTranslationTraits* TranslatorTraits = GetConnectTranslationTraits(ExternalAuthToken.Type);
	if (TranslatorTraits == nullptr)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("FEOSConnectLoginOptions::Create: Failed - Unsupported external auth type: %s"), *ExternalAuthToken.Type.ToString());
		return TDefaultErrorResultInternal<FEOSConnectLoginOptions>(Errors::InvalidParams());
	}

	FEOSConnectLoginOptions EOSConnectLoginOptions;
	if (EnumHasAnyFlags(TranslatorTraits->Flags, EEOSConnectTranslationFlags::DisplayName))
	{
		// Todo: Lookup platform OSS Auth interface to resolve display name using PlatformUserId.
		FCStringAnsi::Strncpy(EOSConnectLoginOptions.DisplayNameUtf8, "PlatformUser", sizeof(EOSConnectLoginOptions.DisplayNameUtf8));
		EOSConnectLoginOptions.UserLoginInfo = &EOSConnectLoginOptions.UserLoginInfoData;
	}

	EOSConnectLoginOptions.ExternalAuthTokenUtf8.SetNumZeroed(ExternalAuthToken.Data.Len() + 1);
	FCStringAnsi::Strncpy(EOSConnectLoginOptions.ExternalAuthTokenUtf8.GetData(), TCHAR_TO_UTF8(*ExternalAuthToken.Data), EOSConnectLoginOptions.ExternalAuthTokenUtf8.Num());
	EOSConnectLoginOptions.CredentialsData.Token = EOSConnectLoginOptions.ExternalAuthTokenUtf8.GetData();
	EOSConnectLoginOptions.CredentialsData.Type = TranslatorTraits->Type;
	return TDefaultErrorResultInternal<FEOSConnectLoginOptions>(MoveTemp(EOSConnectLoginOptions));
}

enum class EEOSAuthTranslationFlags : uint8
{
	None = 0,
	SetId = 1 << 0,
	SetTokenFromString = 1 << 1,
	SetTokenFromExternalAuth = 1 << 2,
};
ENUM_CLASS_FLAGS(EEOSAuthTranslationFlags);

struct FEOSAuthTranslationTraits
{
	EOS_ELoginCredentialType Type;
	EEOSAuthTranslationFlags Flags;
};

struct FEOSExternalAuthTranslationTraits
{
	EOS_EExternalCredentialType Type;
};

class FEOSAuthLoginOptions : public EOS_Auth_LoginOptions
{
public:
	FEOSAuthLoginOptions(const FEOSAuthLoginOptions&) = delete;
	FEOSAuthLoginOptions& operator=(const FEOSAuthLoginOptions&) = delete;
	FEOSAuthLoginOptions(FEOSAuthLoginOptions&&);
	FEOSAuthLoginOptions& operator=(FEOSAuthLoginOptions&&);

	static TDefaultErrorResultInternal<FEOSAuthLoginOptions> Create(
		FName CredentialsType,
		const FString& CredentialsId,
		const TVariant<FString, FExternalAuthToken>& CredentialsToken,
		const TArray<FString>& Scopes);

private:
	static TDefaultErrorResultInternal<FEOSAuthLoginOptions> CreateImpl(
		FName CredentialsType,
		const FString& CredentialsId,
		const TVariant<FString, FExternalAuthToken>& CredentialsToken,
		const TArray<FString>& Scopes);

	FEOSAuthLoginOptions();

	static const FEOSAuthTranslationTraits* GetLoginTranslatorTraits(FName Name);
	static const FEOSExternalAuthTranslationTraits* GetExternalAuthTranslationTraits(FName ExternalAuthType);
	static const EOS_EAuthScopeFlags* GetScopeFlag(const FString& ScopeName);

	EOS_Auth_Credentials CredentialsData;
	TArray<char> IdUtf8;
	TArray<char> TokenUtf8;
};

const FEOSAuthTranslationTraits* FEOSAuthLoginOptions::GetLoginTranslatorTraits(FName Name)
{
	static const TMap<FName, FEOSAuthTranslationTraits> SupportedLoginTranslatorTraits = {
		{ LoginCredentialsType::Password, { EOS_ELoginCredentialType::EOS_LCT_Password, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::ExchangeCode, { EOS_ELoginCredentialType::EOS_LCT_ExchangeCode, EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::PersistentAuth, { EOS_ELoginCredentialType::EOS_LCT_PersistentAuth, EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::Developer, { EOS_ELoginCredentialType::EOS_LCT_Developer, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::RefreshToken, { EOS_ELoginCredentialType::EOS_LCT_RefreshToken, EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::AccountPortal, { EOS_ELoginCredentialType::EOS_LCT_AccountPortal, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::ExternalAuth, { EOS_ELoginCredentialType::EOS_LCT_ExternalAuth, EEOSAuthTranslationFlags::SetTokenFromExternalAuth } },
	};

	return SupportedLoginTranslatorTraits.Find(Name);
}

const FEOSExternalAuthTranslationTraits* FEOSAuthLoginOptions::GetExternalAuthTranslationTraits(FName ExternalAuthType)
{
	static const TMap<FName, FEOSExternalAuthTranslationTraits> SupportedExternalAuthTraits = {
		{ ExternalLoginType::Epic, { EOS_EExternalCredentialType::EOS_ECT_EPIC } },
		{ ExternalLoginType::SteamAppTicket, { EOS_EExternalCredentialType::EOS_ECT_STEAM_APP_TICKET } },
		{ ExternalLoginType::PsnIdToken, { EOS_EExternalCredentialType::EOS_ECT_PSN_ID_TOKEN } },
		{ ExternalLoginType::XblXstsToken, { EOS_EExternalCredentialType::EOS_ECT_XBL_XSTS_TOKEN } },
		{ ExternalLoginType::DiscordAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DISCORD_ACCESS_TOKEN } },
		{ ExternalLoginType::GogSessionTicket, { EOS_EExternalCredentialType::EOS_ECT_GOG_SESSION_TICKET } },
		{ ExternalLoginType::NintendoIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_ID_TOKEN } },
		{ ExternalLoginType::NintendoNsaIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_NSA_ID_TOKEN } },
		{ ExternalLoginType::UplayAccessToken, { EOS_EExternalCredentialType::EOS_ECT_UPLAY_ACCESS_TOKEN } },
		{ ExternalLoginType::OpenIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_OPENID_ACCESS_TOKEN } },
		{ ExternalLoginType::DeviceIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN } },
		{ ExternalLoginType::AppleIdToken, { EOS_EExternalCredentialType::EOS_ECT_APPLE_ID_TOKEN } },
		{ ExternalLoginType::GoogleIdToken, { EOS_EExternalCredentialType::EOS_ECT_GOOGLE_ID_TOKEN } },
		{ ExternalLoginType::OculusUserIdNonce, { EOS_EExternalCredentialType::EOS_ECT_OCULUS_USERID_NONCE } },
		{ ExternalLoginType::ItchioJwt, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_JWT } },
		{ ExternalLoginType::ItchioKey, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_KEY } },
		{ ExternalLoginType::EpicIdToken, { EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN } },
		{ ExternalLoginType::AmazonAccessToken, { EOS_EExternalCredentialType::EOS_ECT_AMAZON_ACCESS_TOKEN } },
	};

	return SupportedExternalAuthTraits.Find(ExternalAuthType);
}

const EOS_EAuthScopeFlags* FEOSAuthLoginOptions::GetScopeFlag(const FString& ScopeName)
{
	static const TMap<FString, EOS_EAuthScopeFlags> SupportedScopes = {
		{ TEXT("BasicProfile"), EOS_EAuthScopeFlags::EOS_AS_BasicProfile },
		{ TEXT("FriendsList"), EOS_EAuthScopeFlags::EOS_AS_FriendsList },
		{ TEXT("Presence"), EOS_EAuthScopeFlags::EOS_AS_Presence },
		{ TEXT("FriendsManagement"), EOS_EAuthScopeFlags::EOS_AS_FriendsManagement },
		{ TEXT("Email"), EOS_EAuthScopeFlags::EOS_AS_Email },
	};

	return SupportedScopes.Find(ScopeName);
}

FEOSAuthLoginOptions::FEOSAuthLoginOptions(FEOSAuthLoginOptions&& Other)
{
	*this = MoveTemp(Other);
}

FEOSAuthLoginOptions& FEOSAuthLoginOptions::operator=(FEOSAuthLoginOptions&& Other)
{
	CredentialsData = Other.CredentialsData;

	// Pointer fixup.
	if (CredentialsData.Id)
	{
		IdUtf8 = MoveTemp(Other.IdUtf8);
		CredentialsData.Id = IdUtf8.GetData();
	}
	if (CredentialsData.Token)
	{
		TokenUtf8 = MoveTemp(Other.TokenUtf8);
		CredentialsData.Token = TokenUtf8.GetData();
	}
	if (CredentialsData.SystemAuthCredentialsOptions)
	{
		// todo
	}

	Credentials = &CredentialsData;
	ApiVersion = Other.ApiVersion;
	ScopeFlags = Other.ScopeFlags;

	Other.Credentials = nullptr;
	return *this;
}

FEOSAuthLoginOptions::FEOSAuthLoginOptions()
{
	// EOS_Auth_LoginOptions init
	static_assert(EOS_AUTH_LOGIN_API_LATEST == 2, "EOS_Auth_LoginOptions updated, check new fields");
	ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	Credentials = &CredentialsData;
	ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_NoFlags;

	// EOS_Auth_Credentials init
	static_assert(EOS_AUTH_CREDENTIALS_API_LATEST == 3, "EOS_Auth_Credentials updated, check new fields");
	CredentialsData.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	CredentialsData.Id = nullptr;
	CredentialsData.Token = nullptr;
	CredentialsData.Type = EOS_ELoginCredentialType::EOS_LCT_Password;
	CredentialsData.SystemAuthCredentialsOptions = nullptr;
	CredentialsData.ExternalType = EOS_EExternalCredentialType::EOS_ECT_EPIC;
}

TDefaultErrorResultInternal<FEOSAuthLoginOptions> FEOSAuthLoginOptions::Create(
	FName CredentialsType,
	const FString& CredentialsId,
	const TVariant<FString, FExternalAuthToken>& CredentialsToken,
	const TArray<FString>& Scopes)
{
	if (CredentialsType == LoginCredentialsType::Auto)
	{
		FString	CommandLineAuthType;
		FString	CommandLineAuthId;
		FString	CommandLineAuthToken;
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), CommandLineAuthType);
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_LOGIN="), CommandLineAuthId);
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), CommandLineAuthToken);
		if (!CommandLineAuthId.IsEmpty() && !CommandLineAuthToken.IsEmpty() && !CommandLineAuthType.IsEmpty())
		{
			return CreateImpl(
				*CommandLineAuthType,
				CommandLineAuthId,
				TVariant<FString, FExternalAuthToken>(TInPlaceType<FString>(),
				MoveTemp(CommandLineAuthToken)),
				Scopes);
		}
		else
		{
			return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidCreds());
		}
	}
	else
	{
		return CreateImpl(CredentialsType, CredentialsId, CredentialsToken, Scopes);
	}
}

TDefaultErrorResultInternal<FEOSAuthLoginOptions> FEOSAuthLoginOptions::CreateImpl(
	FName CredentialsType,
	const FString& CredentialsId,
	const TVariant<FString, FExternalAuthToken>& CredentialsToken,
	const TArray<FString>& Scopes)
{
	const FEOSAuthTranslationTraits* TranslatorTraits = GetLoginTranslatorTraits(CredentialsType);
	if (TranslatorTraits == nullptr)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("FEOSAuthLoginOptions::Create: Failed - Unsupported login type: %s"), *CredentialsType.ToString());
		return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidParams());
	}

	FEOSAuthLoginOptions EOSAuthLoginOptions;
	EOSAuthLoginOptions.CredentialsData.Type = TranslatorTraits->Type;

	if (EnumHasAnyFlags(TranslatorTraits->Flags, EEOSAuthTranslationFlags::SetId))
	{
		const FTCHARToUTF8 Converter(*CredentialsId);
		EOSAuthLoginOptions.IdUtf8.SetNumZeroed(Converter.Length() + 1);
		FCStringAnsi::Strncpy(EOSAuthLoginOptions.IdUtf8.GetData(), Converter.Get(), EOSAuthLoginOptions.IdUtf8.Num());
		EOSAuthLoginOptions.CredentialsData.Id = EOSAuthLoginOptions.IdUtf8.GetData();
	}

	if (EnumHasAnyFlags(TranslatorTraits->Flags, EEOSAuthTranslationFlags::SetTokenFromString))
	{
		if (!CredentialsToken.IsType<FString>())
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("FEOSAuthLoginOptions::Create: Failed - Expected CredentialsToken set to string for Login type: %s"), *CredentialsType.ToString());
			return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidCreds());
		}

		const FTCHARToUTF8 Converter(*CredentialsToken.Get<FString>());
		EOSAuthLoginOptions.TokenUtf8.SetNumZeroed(Converter.Length() + 1);
		FCStringAnsi::Strncpy(EOSAuthLoginOptions.TokenUtf8.GetData(), Converter.Get(), EOSAuthLoginOptions.TokenUtf8.Num());
		EOSAuthLoginOptions.CredentialsData.Token = EOSAuthLoginOptions.TokenUtf8.GetData();
	}

	if (EnumHasAnyFlags(TranslatorTraits->Flags, EEOSAuthTranslationFlags::SetTokenFromExternalAuth))
	{
		if (!CredentialsToken.IsType<FExternalAuthToken>())
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("FEOSAuthLoginOptions::Create: Failed - Expected CredentialsToken set to ExternalAuthToken for Login type: %s"), *CredentialsType.ToString());
			return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidCreds());
		}

		const FExternalAuthToken& ExternalAuthToken = CredentialsToken.Get<FExternalAuthToken>();
		const FEOSExternalAuthTranslationTraits* ExternalAuthTranslationTraits = GetExternalAuthTranslationTraits(ExternalAuthToken.Type);
		if (ExternalAuthTranslationTraits == nullptr)
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("FEOSAuthLoginOptions::Create: Failed - Unsuppoerted external auth type: %s"), *ExternalAuthToken.Type.ToString());
			return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidCreds());
		}

		EOSAuthLoginOptions.TokenUtf8.SetNumZeroed(ExternalAuthToken.Data.Len() + 1);
		FCStringAnsi::Strncpy(EOSAuthLoginOptions.TokenUtf8.GetData(), TCHAR_TO_UTF8(*ExternalAuthToken.Data), EOSAuthLoginOptions.TokenUtf8.Num());
		EOSAuthLoginOptions.CredentialsData.Token = EOSAuthLoginOptions.TokenUtf8.GetData();
		EOSAuthLoginOptions.CredentialsData.ExternalType = ExternalAuthTranslationTraits->Type;
	}

	// todo: handle SystemAuthCredentialsOptions

	// Translate scopes.
	bool bAllScopesValid = true;
	for (const FString& Scope : Scopes)
	{
		if (const EOS_EAuthScopeFlags* ScopeFlag = GetScopeFlag(Scope))
		{
			EOSAuthLoginOptions.ScopeFlags |= *ScopeFlag;
		}
		else
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("FEOSAuthLoginOptions::Create: Failed - Unknown scope: %s"), *Scope);
			bAllScopesValid = false;
		}
	}
	if (!bAllScopesValid)
	{
		return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(Errors::InvalidCreds());
	}

	return TDefaultErrorResultInternal<FEOSAuthLoginOptions>(MoveTemp(EOSAuthLoginOptions));
}

TSharedPtr<FAccountInfoEOS> FAccountInfoRegistryEOS::Find(FPlatformUserId PlatformUserId) const
{
	return StaticCastSharedPtr<FAccountInfoEOS>(Super::Find(PlatformUserId));
}

TSharedPtr<FAccountInfoEOS> FAccountInfoRegistryEOS::Find(FAccountId AccountId) const
{
	return StaticCastSharedPtr<FAccountInfoEOS>(Super::Find(AccountId));
}

TSharedPtr<FAccountInfoEOS> FAccountInfoRegistryEOS::Find(EOS_EpicAccountId EpicAccountId) const
{
	FReadScopeLock Lock(IndexLock);
	if (const TSharedRef<FAccountInfoEOS>* FoundPtr = AuthDataByEpicAccountId.Find(EpicAccountId))
	{
		return *FoundPtr;
	}
	else
	{
		return nullptr;
	}
}

TSharedPtr<FAccountInfoEOS> FAccountInfoRegistryEOS::Find(EOS_ProductUserId ProductUserId) const
{
	FReadScopeLock Lock(IndexLock);
	if (const TSharedRef<FAccountInfoEOS>* FoundPtr = AuthDataByProductUserId.Find(ProductUserId))
	{
		return *FoundPtr;
	}
	else
	{
		return nullptr;
	}
}

void FAccountInfoRegistryEOS::Register(const TSharedRef<FAccountInfoEOS>& AccountInfoEOS)
{
	FWriteScopeLock Lock(IndexLock);
	DoRegister(AccountInfoEOS);
}

void FAccountInfoRegistryEOS::Unregister(FAccountId AccountId)
{
	if (TSharedPtr<FAccountInfoEOS> AccountInfoEOS = Find(AccountId))
	{
		FWriteScopeLock Lock(IndexLock);
		DoUnregister(AccountInfoEOS.ToSharedRef());
	}
	else
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[FAccountInfoRegistryEOS::Unregister] Failed to find account [%s]."), *ToLogString(AccountId));
	}
}

void FAccountInfoRegistryEOS::DoRegister(const TSharedRef<FAccountInfo>& AccountInfo)
{
	TSharedRef<FAccountInfoEOS> AccountInfoEOS = StaticCastSharedRef<FAccountInfoEOS>(AccountInfo);

	Super::DoRegister(AccountInfo);

	if (AccountInfoEOS->EpicAccountId)
	{
		AuthDataByEpicAccountId.Add(AccountInfoEOS->EpicAccountId, AccountInfoEOS);
	}

	if (AccountInfoEOS->ProductUserId)
	{
		AuthDataByProductUserId.Add(AccountInfoEOS->ProductUserId, AccountInfoEOS);
	}
}
void FAccountInfoRegistryEOS::DoUnregister(const TSharedRef<FAccountInfo>& AccountInfo)
{
	TSharedRef<FAccountInfoEOS> AccountInfoEOS = StaticCastSharedRef<FAccountInfoEOS>(AccountInfo);

	Super::DoUnregister(AccountInfo);

	if (AccountInfoEOS->EpicAccountId)
	{
		AuthDataByEpicAccountId.Remove(AccountInfoEOS->EpicAccountId);
	}

	if (AccountInfoEOS->ProductUserId)
	{
		AuthDataByProductUserId.Remove(AccountInfoEOS->ProductUserId);
	}
}

FAuthEOSGS::FAuthEOSGS(FOnlineServicesEOSGS& InServices)
	: Super(InServices)
{
}

void FAuthEOSGS::Initialize()
{
	Super::Initialize();

	RegisterHandlers();

	AuthHandle = EOS_Platform_GetAuthInterface(static_cast<FOnlineServicesEOSGS&>(GetServices()).GetEOSPlatformHandle());
	check(AuthHandle != nullptr);

	ConnectHandle = EOS_Platform_GetConnectInterface(static_cast<FOnlineServicesEOSGS&>(GetServices()).GetEOSPlatformHandle());
	check(ConnectHandle != nullptr);
}

void FAuthEOSGS::PreShutdown()
{
	Super::PreShutdown();

	UnregisterHandlers();
}

TOnlineAsyncOpHandle<FAuthLogin> FAuthEOSGS::Login(FAuthLogin::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthLogin> Op = GetOp<FAuthLogin>(MoveTemp(Params));

	// Login in AuthEOSGS supports calling "Login" again with an updated external auth token to
	// avoid service interruption when the connect token expires.

	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const FAuthLogin::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.PlatformUserId);
		if (!AccountInfoEOS)
		{
			AccountInfoEOS = MakeShared<FAccountInfoEOS>();
			AccountInfoEOS->PlatformUserId = Params.PlatformUserId;
			AccountInfoEOS->LoginStatus = ELoginStatus::NotLoggedIn;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Optionally handle EAS login.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);
		FAuthEOSGSLoginConfig AuthEOSGSLoginConfig;
		LoadConfig(AuthEOSGSLoginConfig, TEXT("Login"));
		
		if (AuthEOSGSLoginConfig.EASAuthEnabled)
		{
			if (!IsOnlineStatus(AccountInfoEOS->LoginStatus))
			{
				TPromise<void> Promise;
				TFuture<void> Future = Promise.GetFuture();

				const FAuthLogin::Params& Params = InAsyncOp.GetParams();

				FAuthLoginEASImpl::Params LoginParams;
				LoginParams.PlatformUserId = Params.PlatformUserId;
				LoginParams.CredentialsType = Params.CredentialsType;
				LoginParams.CredentialsId = Params.CredentialsId;
				LoginParams.CredentialsToken = Params.CredentialsToken;
				LoginParams.Scopes = !Params.Scopes.IsEmpty() ? Params.Scopes : AuthEOSGSLoginConfig.DefaultScopes;

				return LoginEASImpl(LoginParams)
				.Next([this, Promise = MoveTemp(Promise), Op = InAsyncOp.AsShared()](TDefaultErrorResult<FAuthLoginEASImpl>&& LoginResult) mutable -> void
				{
					const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(*Op, AccountInfoKeyName);

					if (LoginResult.IsError())
					{
						UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: LoginEASImpl %s"), *LoginResult.GetErrorValue().GetLogString());
						Op->SetError(Errors::Unknown(MoveTemp(LoginResult.GetErrorValue())));
					}
					else
					{
						// Cache EpicAccountId on successful EAS login.
						AccountInfoEOS->EpicAccountId = LoginResult.GetOkValue().EpicAccountId;
					}

					Promise.EmplaceValue();
				});

				return Future;
			}
			else
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: EAS login enabled and already logged in."));
				InAsyncOp.SetError(Errors::Auth::AlreadyLoggedIn());
			}
		}

		return MakeFulfilledPromise<void>().GetFuture();
	})
	// Step 3: Fetch external auth credentials for connect login.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const FAuthLogin::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		TPromise<FAuthLoginConnectImpl::Params> Promise;
		TFuture<FAuthLoginConnectImpl::Params> Future = Promise.GetFuture();

		// When an epic account is present, use the external auth credentials of the logged in account.
		if (AccountInfoEOS->EpicAccountId)
		{
			TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> AuthTokenResult = GetExternalAuthTokenImpl(FAuthGetExternalAuthTokenImpl::Params{AccountInfoEOS->EpicAccountId});
			if (AuthTokenResult.IsError())
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: GetExternalAuthTokenImpl %s"), *AuthTokenResult.GetErrorValue().GetLogString());
				InAsyncOp.SetError(Errors::Unknown(MoveTemp(AuthTokenResult.GetErrorValue())));

				// Failed to acquire token - logout EAS.
				LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
				.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
				{
					if (LogoutResult.IsError())
					{
						UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
					}
					Promise.EmplaceValue(FAuthLoginConnectImpl::Params{});
				});

				return Future;
			}

			Promise.EmplaceValue(FAuthLoginConnectImpl::Params{Params.PlatformUserId, MoveTemp(AuthTokenResult.GetOkValue().Token)});
		}
		else
		{
			// Connect login requires external auth.
			if (Params.CredentialsType != LoginCredentialsType::ExternalAuth || !Params.CredentialsToken.IsType<FExternalAuthToken>())
			{
				InAsyncOp.SetError(Errors::InvalidParams());
				Promise.EmplaceValue(FAuthLoginConnectImpl::Params{});
				return Future;
			}

			Promise.EmplaceValue(FAuthLoginConnectImpl::Params{Params.PlatformUserId, Params.CredentialsToken.Get<FExternalAuthToken>()});
		}

		return Future;
	})
	// Step 4: Attempt connect login. On connect login failure handle logout of EAS.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp, FAuthLoginConnectImpl::Params&& LoginConnectParams)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		TPromise<void> Promise;
		TFuture<void> Future = Promise.GetFuture();

		// Attempt connect login.
		LoginConnectImpl(LoginConnectParams)
		.Next([this, AccountInfoEOS, Op = InAsyncOp.AsShared(), Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLoginConnectImpl>&& LoginResult) mutable -> void
		{
			if (LoginResult.IsError())
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: LoginConnectImpl %s"), *LoginResult.GetErrorValue().GetLogString());
				Op->SetError(Errors::Unknown(MoveTemp(LoginResult.GetErrorValue())));

				// Logout of EAS on login failure if necessary.
				if (AccountInfoEOS->EpicAccountId)
				{
					LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
					.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
					{
						if (LogoutResult.IsError())
						{
							UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
						}
						Promise.EmplaceValue();
					});
				}
				else
				{
					Promise.EmplaceValue();
				}
			}
			else
			{
				// Successful login.
				AccountInfoEOS->ProductUserId = LoginResult.GetOkValue().ProductUserId;
				Promise.EmplaceValue();
			}
		});

		return Future;
	})
	// Step 5: Fetch dependent data.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		if (!IsOnlineStatus(AccountInfoEOS->LoginStatus))
		{
			// Set DisplayName.
			int32_t ProductUserIdBufferLength = EOS_PRODUCTUSERID_MAX_LENGTH + 1;
			char ProductUserIdBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
			EOS_EResult ProductUserIdResult = EOS_ProductUserId_ToString(AccountInfoEOS->ProductUserId, ProductUserIdBuffer, &ProductUserIdBufferLength);
			if (ProductUserIdResult == EOS_EResult::EOS_Success)
			{
				AccountInfoEOS->Attributes.Emplace(AccountAttributeData::DisplayName, UTF8_TO_TCHAR(ProductUserIdBuffer));
			}
			else
			{
				FOnlineError ProductUserIdError(Errors::FromEOSResult(ProductUserIdResult));
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: EOS_ProductUserId_ToString %s"), *ProductUserIdError.GetLogString());
				InAsyncOp.SetError(Errors::Unknown(MoveTemp(ProductUserIdError)));

				// Handle EAS logout if needed.
				if (AccountInfoEOS->EpicAccountId)
				{
					TPromise<void> Promise;
					TFuture<void> Future = Promise.GetFuture();

					LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
					.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
					{
						if (LogoutResult.IsError())
						{
							UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
						}
						Promise.EmplaceValue();
					});

					return Future;
				}
			}
		}

		return MakeFulfilledPromise<void>().GetFuture();
	})
	// Step 6: bookkeeping and notifications.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		if (IsOnlineStatus(AccountInfoEOS->LoginStatus))
		{
			UE_LOG(LogOnlineServices, Log, TEXT("[FAuthEOSGS::Login] Successfully refreshed logged in as [%s]"), *ToLogString(AccountInfoEOS->AccountId));
		}
		else
		{
			AccountInfoEOS->LoginStatus = ELoginStatus::LoggedIn;
			AccountInfoEOS->AccountId = CreateAccountId(AccountInfoEOS->ProductUserId);
			AccountInfoRegistryEOS.Register(AccountInfoEOS);

			UE_LOG(LogOnlineServices, Log, TEXT("[FAuthEOSGS::Login] Successfully logged in as [%s]"), *ToLogString(AccountInfoEOS->AccountId));
			OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });
		}

		InAsyncOp.SetResult(FAuthLogin::Result{AccountInfoEOS});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthLogout> FAuthEOSGS::Logout(FAuthLogout::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthLogout> Op = GetOp<FAuthLogout>(MoveTemp(Params));

	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthLogout>& InAsyncOp)
	{
		const FAuthLogout::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalAccountId);
		if (!AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::InvalidUser());
			return;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Delete persistent auth if needed.
	.Then([this](TOnlineAsyncOp<FAuthLogout>& InAsyncOp)
	{
		const FAuthLogout::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		if (Params.bDestroyAuth && AccountInfoEOS->EpicAccountId)
		{
			TPromise<void> Promise;
			TFuture<void> Future = Promise.GetFuture();

			EOS_Auth_DeletePersistentAuthOptions DeletePersistentAuthOptions = { 0 };
			DeletePersistentAuthOptions.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
			DeletePersistentAuthOptions.RefreshToken = nullptr; // TODO: Is this needed?  Docs say it's needed for consoles
			static_assert(EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST == 2, "EOS_Auth_DeletePersistentAuthOptions updated, check new fields");

			EOS_Async(EOS_Auth_DeletePersistentAuth, AuthHandle, DeletePersistentAuthOptions,
			[Promise = MoveTemp(Promise)](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data) mutable -> void
			{
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					FOnlineError DeletePersistentAuthError(Errors::FromEOSResult(Data->ResultCode));
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Logout] Failure: DeletePersistentAuthResult %s"), *DeletePersistentAuthError.GetLogString());
				}

				Promise.EmplaceValue();
			});

			return Future;
		}

		return MakeFulfilledPromise<void>().GetFuture();
	})
	// Step 3: Logout of EAS if necessary.
	.Then([this](TOnlineAsyncOp<FAuthLogout>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		if (AccountInfoEOS->EpicAccountId)
		{
			TPromise<void> Promise;
			TFuture<void> Future = Promise.GetFuture();

			LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
			.Next([this, AccountInfoEOS, Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
			{
				if (LogoutResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::Logout] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
				}
				Promise.EmplaceValue();
			});

			return Future;
		}
		else
		{
			return MakeFulfilledPromise<void>().GetFuture();
		}
	})
	.Then([this](TOnlineAsyncOp<FAuthLogout>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		UE_LOG(LogOnlineServices, Log, TEXT("[FAuthEOSGS::Logout] Successfully logged out [%s]"), *ToLogString(AccountInfoEOS->AccountId));
		AccountInfoEOS->LoginStatus = ELoginStatus::NotLoggedIn;
		OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });
		AccountInfoRegistryEOS.Unregister(AccountInfoEOS->AccountId);

		InAsyncOp.SetResult(FAuthLogout::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthQueryVerifiedAuthTicket> FAuthEOSGS::QueryVerifiedAuthTicket(FAuthQueryVerifiedAuthTicket::Params&& Params)
{
	// Todo
	TOnlineAsyncOpRef<FAuthQueryVerifiedAuthTicket> Operation = GetOp<FAuthQueryVerifiedAuthTicket>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FAuthCancelVerifiedAuthTicket> FAuthEOSGS::CancelVerifiedAuthTicket(FAuthCancelVerifiedAuthTicket::Params&& Params)
{
	// Todo
	TOnlineAsyncOpRef<FAuthCancelVerifiedAuthTicket> Operation = GetOp<FAuthCancelVerifiedAuthTicket>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FAuthBeginVerifiedAuthSession> FAuthEOSGS::BeginVerifiedAuthSession(FAuthBeginVerifiedAuthSession::Params&& Params)
{
	// Todo
	TOnlineAsyncOpRef<FAuthBeginVerifiedAuthSession> Operation = GetOp<FAuthBeginVerifiedAuthSession>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FAuthEndVerifiedAuthSession> FAuthEOSGS::EndVerifiedAuthSession(FAuthEndVerifiedAuthSession::Params&& Params)
{
	// Todo
	TOnlineAsyncOpRef<FAuthEndVerifiedAuthSession> Operation = GetOp<FAuthEndVerifiedAuthSession>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TFuture<FAccountId> FAuthEOSGS::ResolveAccountId(const FAccountId& LocalAccountId, const EOS_ProductUserId ProductUserId)
{
	return MakeFulfilledPromise<FAccountId>(CreateAccountId(ProductUserId)).GetFuture();
}

TFuture<TArray<FAccountId>> FAuthEOSGS::ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_ProductUserId>& InProductUserIds)
{
	TArray<FAccountId> AccountIds;
	AccountIds.Reserve(InProductUserIds.Num());
	for (const EOS_ProductUserId ProductUserId : InProductUserIds)
	{
		AccountIds.Emplace(CreateAccountId(ProductUserId));
	}
	return MakeFulfilledPromise<TArray<FAccountId>>(MoveTemp(AccountIds)).GetFuture();
}

TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_ProductUserId& ProductUserId)> FAuthEOSGS::ResolveProductIdFn()
{
	return [this](FOnlineAsyncOp& InAsyncOp, const EOS_ProductUserId& ProductUserId)
	{
		const FAccountId* LocalAccountIdPtr = InAsyncOp.Data.Get<FAccountId>(TEXT("LocalAccountId"));
		if (!ensure(LocalAccountIdPtr))
		{
			return MakeFulfilledPromise<FAccountId>().GetFuture();
		}
		return ResolveAccountId(*LocalAccountIdPtr, ProductUserId);
	};
}

TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_ProductUserId>& ProductUserIds)> FAuthEOSGS::ResolveProductIdsFn()
{
	return [this](FOnlineAsyncOp& InAsyncOp, const TArray<EOS_ProductUserId>& ProductUserIds)
	{
		const FAccountId* LocalAccountIdPtr = InAsyncOp.Data.Get<FAccountId>(TEXT("LocalAccountId"));
		if (!ensure(LocalAccountIdPtr))
		{
			return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
		}
		return ResolveAccountIds(*LocalAccountIdPtr, ProductUserIds);
	};
}

TFuture<TDefaultErrorResult<FAuthLoginEASImpl>> FAuthEOSGS::LoginEASImpl(const FAuthLoginEASImpl::Params& LoginParams)
{
	TDefaultErrorResultInternal<FEOSAuthLoginOptions> LoginOptionsResult = FEOSAuthLoginOptions::Create(LoginParams.CredentialsType, LoginParams.CredentialsId, LoginParams.CredentialsToken, LoginParams.Scopes);
	if (LoginOptionsResult.IsError())
	{
		return MakeFulfilledPromise<TDefaultErrorResult<FAuthLoginEASImpl>>(MoveTemp(LoginOptionsResult.GetErrorValue())).GetFuture();
	}

	const bool IsPersistentAuthLogin = LoginParams.CredentialsType == LoginCredentialsType::PersistentAuth;

	TPromise<TDefaultErrorResult<FAuthLoginEASImpl>> Promise;
	TFuture<TDefaultErrorResult<FAuthLoginEASImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Auth_Login, AuthHandle, MoveTemp(LoginOptionsResult.GetOkValue()),
	[AuthHandle = AuthHandle, IsPersistentAuthLogin, Promise = MoveTemp(Promise)](const EOS_Auth_LoginCallbackInfo* Data) mutable -> void
	{
		UE_LOG(LogOnlineServices, Verbose, TEXT("[FAuthEOSGS::LoginEASImpl] EOS_Auth_Login Result: [%s]"), *LexToString(Data->ResultCode));

		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			Promise.SetValue(TDefaultErrorResult<FAuthLoginEASImpl>(FAuthLoginEASImpl::Result{ Data->LocalUserId }));
		}
		else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Data->ContinuanceToken != nullptr)
		{
			EOS_Auth_LinkAccountOptions LinkAccountOptions = {};
			LinkAccountOptions.ApiVersion = EOS_AUTH_LINKACCOUNT_API_LATEST;
			LinkAccountOptions.ContinuanceToken = Data->ContinuanceToken;
			static_assert(EOS_AUTH_LINKACCOUNT_API_LATEST == 1, "EOS_Auth_LinkAccountOptions updated, check new fields");

			EOS_Async(EOS_Auth_LinkAccount, AuthHandle, LinkAccountOptions,
			[Promise = MoveTemp(Promise)](const EOS_Auth_LinkAccountCallbackInfo* Data) mutable -> void
			{
				UE_LOG(LogOnlineServices, Verbose, TEXT("[FAuthEOSGS::LoginEASImpl] EOS_Auth_LinkAccount Result: [%s]"), *LexToString(Data->ResultCode));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Promise.SetValue(TDefaultErrorResult<FAuthLoginEASImpl>(FAuthLoginEASImpl::Result{ Data->LocalUserId }));
				}
				else
				{
					Promise.SetValue(TDefaultErrorResult<FAuthLoginEASImpl>(Errors::FromEOSResult(Data->ResultCode)));
				}
			});
		}
		else
		{
			FOnlineError ResolvedError = Errors::FromEOSResult(Data->ResultCode);

			const bool bShouldRemoveCachedToken =
				Data->ResultCode == EOS_EResult::EOS_InvalidAuth ||
				Data->ResultCode == EOS_EResult::EOS_AccessDenied ||
				Data->ResultCode == EOS_EResult::EOS_Auth_InvalidToken;

			// Remove persistent auth credentials when they are found to be invalid.
			if (IsPersistentAuthLogin && bShouldRemoveCachedToken)
			{
				EOS_Auth_DeletePersistentAuthOptions DeletePersistentAuthOptions = {};
				DeletePersistentAuthOptions.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
				static_assert(EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST == 2, "EOS_Auth_DeletePersistentAuthOptions updated, check new fields");

				EOS_Async(EOS_Auth_DeletePersistentAuth, AuthHandle, DeletePersistentAuthOptions,
				[ResolvedError = MoveTemp(ResolvedError), Promise = MoveTemp(Promise)](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data) mutable -> void
				{
					if (Data->ResultCode != EOS_EResult::EOS_Success)
					{
						FOnlineError DeletePersistentAuthError(Errors::FromEOSResult(Data->ResultCode));
						UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::LoginEASImpl] Failure: DeletePersistentAuthResult %s"), *DeletePersistentAuthError.GetLogString());
					}

					Promise.SetValue(TDefaultErrorResult<FAuthLoginEASImpl>(MoveTemp(ResolvedError)));
				});
			}
			else
			{
				Promise.SetValue(TDefaultErrorResult<FAuthLoginEASImpl>(MoveTemp(ResolvedError)));
			}
		}
	});

	return Future;
}

TFuture<TDefaultErrorResult<FAuthLogoutEASImpl>> FAuthEOSGS::LogoutEASImpl(const FAuthLogoutEASImpl::Params& LogoutParams)
{
	TPromise<TDefaultErrorResult<FAuthLogoutEASImpl>> Promise;
	TFuture<TDefaultErrorResult<FAuthLogoutEASImpl>> Future = Promise.GetFuture();

	EOS_Auth_LogoutOptions LogoutOptions = {};
	LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
	LogoutOptions.LocalUserId = LogoutParams.EpicAccountId;
	static_assert(EOS_AUTH_LOGOUT_API_LATEST == 1, "EOS_Auth_LogoutOptions updated, check new fields");

	EOS_Async(EOS_Auth_Logout, AuthHandle, LogoutOptions,
	[Promise = MoveTemp(Promise)](const EOS_Auth_LogoutCallbackInfo* Data) mutable -> void
	{
		UE_LOG(LogOnlineServices, Verbose, TEXT("[FAuthEOSGS::LogoutEASImpl] EOS_Auth_Logout Result: [%s]"), *LexToString(Data->ResultCode));

		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			Promise.SetValue(TDefaultErrorResult<FAuthLogoutEASImpl>(FAuthLogoutEASImpl::Result{}));
		}
		else
		{
			Promise.SetValue(TDefaultErrorResult<FAuthLogoutEASImpl>(Errors::FromEOSResult(Data->ResultCode)));
		}
	});

	return Future;
}

TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> FAuthEOSGS::GetExternalAuthTokenImpl(const FAuthGetExternalAuthTokenImpl::Params& Params)
{
	EOS_Auth_CopyIdTokenOptions CopyIdTokenOptions = {};
	CopyIdTokenOptions.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
	CopyIdTokenOptions.AccountId = Params.EpicAccountId;
	static_assert(EOS_AUTH_COPYIDTOKEN_API_LATEST == 1, "EOS_Auth_CopyIdTokenOptions updated, check new fields");

	EOS_Auth_IdToken* IdToken = nullptr;
	EOS_EResult Result = EOS_Auth_CopyIdToken(AuthHandle, &CopyIdTokenOptions, &IdToken);
	if (Result == EOS_EResult::EOS_Success)
	{
		ON_SCOPE_EXIT
		{
			EOS_Auth_IdToken_Release(IdToken);
		};

		FExternalAuthToken ExternalAuthToken;
		ExternalAuthToken.Type = ExternalLoginType::EpicIdToken;
		ExternalAuthToken.Data = UTF8_TO_TCHAR(IdToken->JsonWebToken);
		return TDefaultErrorResult<FAuthGetExternalAuthTokenImpl>(FAuthGetExternalAuthTokenImpl::Result{ MoveTemp(ExternalAuthToken) });
	}
	else
	{
		return TDefaultErrorResult<FAuthGetExternalAuthTokenImpl>(Errors::FromEOSResult(Result));
	}
}

TFuture<TDefaultErrorResult<FAuthLoginConnectImpl>> FAuthEOSGS::LoginConnectImpl(const FAuthLoginConnectImpl::Params& LoginParams)
{
	TDefaultErrorResultInternal<FEOSConnectLoginOptions> LoginOptionsResult = FEOSConnectLoginOptions::Create(LoginParams.PlatformUserId, LoginParams.ExternalAuthToken);
	if (LoginOptionsResult.IsError())
	{
		return MakeFulfilledPromise<TDefaultErrorResult<FAuthLoginConnectImpl>>(MoveTemp(LoginOptionsResult.GetErrorValue())).GetFuture();
	}

	TPromise<TDefaultErrorResult<FAuthLoginConnectImpl>> Promise;
	TFuture<TDefaultErrorResult<FAuthLoginConnectImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Connect_Login, ConnectHandle, MoveTemp(LoginOptionsResult.GetOkValue()),
	[ConnectHandle = ConnectHandle, Promise = MoveTemp(Promise)](const EOS_Connect_LoginCallbackInfo* Data) mutable -> void
	{
		UE_LOG(LogOnlineServices, Verbose, TEXT("[FAuthEOSGS::LoginConnect] EOS_Connect_Login Result: [%s]"), *LexToString(Data->ResultCode));

		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			Promise.SetValue(TDefaultErrorResult<FAuthLoginConnectImpl>(FAuthLoginConnectImpl::Result{ Data->LocalUserId }));
		}
		else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Data->ContinuanceToken != nullptr)
		{
			EOS_Connect_CreateUserOptions ConnectCreateUserOptions = { };
			ConnectCreateUserOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
			ConnectCreateUserOptions.ContinuanceToken = Data->ContinuanceToken;
			static_assert(EOS_CONNECT_CREATEUSER_API_LATEST == 1, "EOS_Connect_CreateUserOptions updated, check new fields");

			EOS_Async(EOS_Connect_CreateUser, ConnectHandle, ConnectCreateUserOptions,
			[Promise = MoveTemp(Promise)](const EOS_Connect_CreateUserCallbackInfo* Data) mutable -> void
			{
				UE_LOG(LogOnlineServices, Verbose, TEXT("[FAuthEOSGS::LoginConnect] EOS_Connect_CreateUser Result: [%s]"), *LexToString(Data->ResultCode));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Promise.SetValue(TDefaultErrorResult<FAuthLoginConnectImpl>(FAuthLoginConnectImpl::Result{ Data->LocalUserId }));
				}
				else
				{
					Promise.SetValue(TDefaultErrorResult<FAuthLoginConnectImpl>(Errors::FromEOSResult(Data->ResultCode)));
				}
			});
		}
		else
		{
			Promise.SetValue(TDefaultErrorResult<FAuthLoginConnectImpl>(Errors::FromEOSResult(Data->ResultCode)));
		}
	});

	return Future;
}

TOnlineAsyncOpHandle<FAuthConnectLoginRecoveryImpl> FAuthEOSGS::ConnectLoginRecoveryImplOp(FAuthConnectLoginRecoveryImpl::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthConnectLoginRecoveryImpl> Op = GetOp<FAuthConnectLoginRecoveryImpl>(MoveTemp(Params));

	// Step 1: Setup operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthConnectLoginRecoveryImpl>& InAsyncOp)
	{
		const FAuthConnectLoginRecoveryImpl::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalUserId);
		if (!AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::InvalidUser());
			return;
		}

		if (AccountInfoEOS->LoginStatus != ELoginStatus::LoggedInReducedFunctionality)
		{
			InAsyncOp.SetError(Errors::Unknown());
			return;
		}

		if (!ensure(AccountInfoEOS->EpicAccountId))
		{
			InAsyncOp.SetError(Errors::Unknown());
			return;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Acquire EAS external auth token.
	.Then([this](TOnlineAsyncOp<FAuthConnectLoginRecoveryImpl>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> AuthTokenResult = GetExternalAuthTokenImpl(FAuthGetExternalAuthTokenImpl::Params{ AccountInfoEOS->EpicAccountId });
		if (AuthTokenResult.IsError())
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::ConnectLoginRecoveryImplOp] Failure: GetExternalAuthTokenImpl %s"), *AuthTokenResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(Errors::Unknown(MoveTemp(AuthTokenResult.GetErrorValue())));

			// Reinitialize recovery timer.
			InitializeConnectLoginRecoveryTimer(AccountInfoEOS);
			return FAuthLoginConnectImpl::Params{};
		}

		return FAuthLoginConnectImpl::Params{ AccountInfoEOS->PlatformUserId, MoveTemp(AuthTokenResult.GetOkValue().Token) };
	})
	// Step 3: Refresh connect login.
	.Then([this](TOnlineAsyncOp<FAuthConnectLoginRecoveryImpl>& InAsyncOp, FAuthLoginConnectImpl::Params&& LoginConnectParams)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		TPromise<void> Promise;
		TFuture<void> Future = Promise.GetFuture();

		// Attempt connect login.
		LoginConnectImpl(LoginConnectParams)
		.Next([this, AccountInfoEOS, Op = InAsyncOp.AsShared(), Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLoginConnectImpl>&& LoginResult) mutable -> void
		{
			if (LoginResult.IsError())
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::ConnectLoginRecoveryImplOp] Failure: LoginConnectImpl %s"), *LoginResult.GetErrorValue().GetLogString());
				Op->SetError(Errors::Unknown(MoveTemp(LoginResult.GetErrorValue())));

				// Reinitialize recovery timer.
				InitializeConnectLoginRecoveryTimer(AccountInfoEOS);
				Promise.EmplaceValue();
			}
			else
			{
				// Successful login.
				Promise.EmplaceValue();
			}
		});

		return Future;
	})
	// Step 4: Update cache and notify.
	.Then([this](TOnlineAsyncOp<FAuthConnectLoginRecoveryImpl>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		UE_LOG(LogOnlineServices, Log, TEXT("[FAuthEOSGS::ConnectLoginRecoveryImplOp] User has restored online capability [%s]"), *ToLogString(AccountInfoEOS->AccountId));
		AccountInfoEOS->LoginStatus = ELoginStatus::LoggedIn;
		OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });

		InAsyncOp.SetResult(FAuthConnectLoginRecoveryImpl::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthHandleConnectLoginStatusChangedImpl> FAuthEOSGS::HandleConnectLoginStatusChangedImplOp(FAuthHandleConnectLoginStatusChangedImpl::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthHandleConnectLoginStatusChangedImpl> Op = GetOp<FAuthHandleConnectLoginStatusChangedImpl>(MoveTemp(Params));

	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthHandleConnectLoginStatusChangedImpl>& InAsyncOp)
	{
		const FAuthHandleConnectLoginStatusChangedImpl::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalUserId);
		if (!AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::InvalidUser());
			return;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Update status and notify.
	.Then([this](TOnlineAsyncOp<FAuthHandleConnectLoginStatusChangedImpl>& InAsyncOp)
	{
		const FAuthHandleConnectLoginStatusChangedImpl::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		// Check if the user was kicked offline.
		if (Params.CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn)
		{
			// Check if user can be brought back online automatically.
			if (AccountInfoEOS->EpicAccountId)
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::HandleConnectLoginStatusChangedImplOp] User has reduced online capability [%s]"), *ToLogString(AccountInfoEOS->AccountId));
				AccountInfoEOS->LoginStatus = ELoginStatus::LoggedInReducedFunctionality;
				OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });

				// Auth has now entered degraded state for the user. Set timer to periodically attempt to reestablish full auth connection.
				InitializeConnectLoginRecoveryTimer(AccountInfoEOS);
			}
			else
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::HandleConnectLoginStatusChangedImplOp] User is offline [%s]"), *ToLogString(AccountInfoEOS->AccountId));
				AccountInfoEOS->LoginStatus = ELoginStatus::NotLoggedIn;
				OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });
				AccountInfoRegistryEOS.Unregister(AccountInfoEOS->AccountId);
			}
		}

		InAsyncOp.SetResult(FAuthHandleConnectLoginStatusChangedImpl::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthHandleConnectAuthNotifyExpirationImpl> FAuthEOSGS::HandleConnectAuthNotifyExpirationImplOp(FAuthHandleConnectAuthNotifyExpirationImpl::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthHandleConnectAuthNotifyExpirationImpl> Op = GetOp<FAuthHandleConnectAuthNotifyExpirationImpl>(MoveTemp(Params));

	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthHandleConnectAuthNotifyExpirationImpl>& InAsyncOp)
	{
		const FAuthHandleConnectAuthNotifyExpirationImpl::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalUserId);
		if (!AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::InvalidUser());
			return;
		}

		if (AccountInfoEOS->LoginStatus != ELoginStatus::LoggedIn)
		{
			InAsyncOp.SetError(Errors::NotLoggedIn());
			return;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Automatically start recovery or notify user to start recovery.
	.Then([this](TOnlineAsyncOp<FAuthHandleConnectAuthNotifyExpirationImpl>& InAsyncOp)
	{
		const FAuthHandleConnectAuthNotifyExpirationImpl::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		// When EAS is logged in use it to refresh the connect login.
		if (AccountInfoEOS->EpicAccountId)
		{
			// Auth has now entered degraded state for the user. Set timer to periodically attempt to reestablish full auth connection.
			InitializeConnectLoginRecoveryTimer(AccountInfoEOS);
		}
		else
		{
			// No EAS login, notify user code to refresh connect login using external auth.
			OnAuthPendingAuthExpirationEvent.Broadcast(FAuthPendingAuthExpiration{ AccountInfoEOS });
		}

		InAsyncOp.SetResult(FAuthHandleConnectAuthNotifyExpirationImpl::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthHandleEASLoginStatusChangedImpl> FAuthEOSGS::HandleEASLoginStatusChangedImplOp(FAuthHandleEASLoginStatusChangedImpl::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthHandleEASLoginStatusChangedImpl> Op = GetOp<FAuthHandleEASLoginStatusChangedImpl>(MoveTemp(Params));

	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthHandleEASLoginStatusChangedImpl>& InAsyncOp)
	{
		const FAuthHandleEASLoginStatusChangedImpl::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalUserId);
		if (!AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::InvalidUser());
			return;
		}

		if (AccountInfoEOS->LoginStatus != ELoginStatus::LoggedIn)
		{
			InAsyncOp.SetError(Errors::NotLoggedIn());
			return;
		}

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(AccountInfoKeyName, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Update status and notify.
	.Then([this](TOnlineAsyncOp<FAuthHandleEASLoginStatusChangedImpl>& InAsyncOp)
	{
		const FAuthHandleEASLoginStatusChangedImpl::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, AccountInfoKeyName);

		// Check if the user was kicked offline.
		if (Params.CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn)
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOSGS::HandleEASLoginStatusChangedImplOp] User is now offline [%s]"), *ToLogString(AccountInfoEOS->AccountId));
			AccountInfoEOS->LoginStatus = ELoginStatus::NotLoggedIn;
			OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });
			AccountInfoRegistryEOS.Unregister(AccountInfoEOS->AccountId);
		}

		InAsyncOp.SetResult(FAuthHandleEASLoginStatusChangedImpl::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

void FAuthEOSGS::RegisterHandlers()
{
	// Register for EOS connect connection status updates.
	OnConnectLoginStatusChangedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		ConnectHandle,
		EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST,
		&EOS_Connect_AddNotifyLoginStatusChanged,
		&EOS_Connect_RemoveNotifyLoginStatusChanged,
		&FAuthEOSGS::OnConnectLoginStatusChanged);
	static_assert(EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST == 1, "EOS_Connect_AddNotifyLoginStatusChanged updated, check new fields");

	// Notification of a pending external token expiration ~10 minutes.
	OnConnectAuthNotifyExpirationEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		ConnectHandle,
		EOS_CONNECT_ONAUTHEXPIRATIONCALLBACK_API_LATEST,
		&EOS_Connect_AddNotifyAuthExpiration,
		&EOS_Connect_RemoveNotifyAuthExpiration,
		&FAuthEOSGS::OnConnectAuthNotifyExpiration);
	static_assert(EOS_CONNECT_ONAUTHEXPIRATIONCALLBACK_API_LATEST == 1, "EOS_Connect_AddNotifyAuthExpiration updated, check new fields");

	// Register for EAS connection status updates.
	OnConnectAuthNotifyExpirationEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		AuthHandle,
		EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST,
		&EOS_Auth_AddNotifyLoginStatusChanged,
		&EOS_Auth_RemoveNotifyLoginStatusChanged,
		&FAuthEOSGS::OnEASLoginStatusChanged);
	static_assert(EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST == 1, "EOS_Auth_AddNotifyLoginStatusChanged updated, check new fields");
}

void FAuthEOSGS::UnregisterHandlers()
{
	OnConnectLoginStatusChangedEOSEventRegistration = nullptr;
	OnConnectAuthNotifyExpirationEOSEventRegistration = nullptr;
	OnConnectAuthNotifyExpirationEOSEventRegistration = nullptr;
}

void FAuthEOSGS::OnConnectLoginStatusChanged(const EOS_Connect_LoginStatusChangedCallbackInfo* Data)
{
	HandleConnectLoginStatusChangedImplOp(FAuthHandleConnectLoginStatusChangedImpl::Params{ Data->LocalUserId, Data->PreviousStatus, Data->CurrentStatus });
}

void FAuthEOSGS::OnConnectAuthNotifyExpiration(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
	HandleConnectAuthNotifyExpirationImplOp(FAuthHandleConnectAuthNotifyExpirationImpl::Params{ Data->LocalUserId });
}

void FAuthEOSGS::OnEASLoginStatusChanged(const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
{
	HandleEASLoginStatusChangedImplOp(FAuthHandleEASLoginStatusChangedImpl::Params{ Data->LocalUserId, Data->PrevStatus, Data->CurrentStatus });
}

#if !UE_BUILD_SHIPPING
void FAuthEOSGS::CheckMetadata()
{
	// Metadata sanity check.
	ToLogString(FAuthLoginEASImpl::Params());
	ToLogString(FAuthLoginEASImpl::Result());
	ToLogString(FAuthLogoutEASImpl::Params());
	ToLogString(FAuthLogoutEASImpl::Result());
	ToLogString(FAuthGetExternalAuthTokenImpl::Params());
	ToLogString(FAuthGetExternalAuthTokenImpl::Result());
	ToLogString(FAuthLoginConnectImpl::Params());
	ToLogString(FAuthLoginConnectImpl::Result());
	ToLogString(FAuthConnectLoginRecoveryImpl::Params());
	ToLogString(FAuthConnectLoginRecoveryImpl::Result());
	ToLogString(FAuthHandleConnectLoginStatusChangedImpl::Params());
	ToLogString(FAuthHandleConnectLoginStatusChangedImpl::Result());
	ToLogString(FAuthHandleConnectAuthNotifyExpirationImpl::Params());
	ToLogString(FAuthHandleConnectAuthNotifyExpirationImpl::Result());
	ToLogString(FAuthHandleEASLoginStatusChangedImpl::Params());
	ToLogString(FAuthHandleEASLoginStatusChangedImpl::Result());
	ToLogString(FAccountInfoEOS());
	Meta::VisitFields(FAuthLoginEASImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthLoginEASImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthLogoutEASImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthLogoutEASImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthGetExternalAuthTokenImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthGetExternalAuthTokenImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthLoginConnectImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthLoginConnectImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthConnectLoginRecoveryImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthConnectLoginRecoveryImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleConnectLoginStatusChangedImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleConnectLoginStatusChangedImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleConnectAuthNotifyExpirationImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleConnectAuthNotifyExpirationImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleEASLoginStatusChangedImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FAuthHandleEASLoginStatusChangedImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
}
#endif

const FAccountInfoRegistry& FAuthEOSGS::GetAccountInfoRegistry() const
{
	return AccountInfoRegistryEOS;
}

void FAuthEOSGS::InitializeConnectLoginRecoveryTimer(const TSharedRef<FAccountInfoEOS>& AccountInfoEOS)
{
	FAuthEOSGSLoginRecoveryConfig AuthEOSGSLoginRecoveryConfig;
	LoadConfig(AuthEOSGSLoginRecoveryConfig, TEXT("LoginRecovery"));
	const float DelayTime = AuthEOSGSLoginRecoveryConfig.Interval + AuthEOSGSLoginRecoveryConfig.Jitter * FMath::RandRange(0.f, 1.f);

	if (AccountInfoEOS->RestoreLoginTimer.IsValid())
	{
		FTSBackgroundableTicker::GetCoreTicker().RemoveTicker(AccountInfoEOS->RestoreLoginTimer);
	}

	AccountInfoEOS->RestoreLoginTimer = FTSBackgroundableTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this, EpicAccountId = AccountInfoEOS->EpicAccountId](float) -> bool
		{
			ConnectLoginRecoveryImplOp(FAuthConnectLoginRecoveryImpl::Params{ EpicAccountId });

			// One-shot timer.
			return false;
		}),
		DelayTime);
}

FAccountId FAuthEOSGS::CreateAccountId(const EOS_ProductUserId ProductUserId)
{
	return FOnlineAccountIdRegistryEOSGS::Get().FindOrAddAccountId(ProductUserId);
}

/* UE::Online */ }
