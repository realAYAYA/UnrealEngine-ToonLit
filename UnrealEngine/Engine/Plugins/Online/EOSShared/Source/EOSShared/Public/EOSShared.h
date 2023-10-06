// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EOS_SDK

#include "HAL/PreprocessorHelpers.h"

#ifndef UE_WITH_EOS_SDK_APIVERSION_WARNINGS
#define UE_WITH_EOS_SDK_APIVERSION_WARNINGS 1
#endif

/**
 * Emits warnings when an SDK upgrade bumps the ApiVersion, to let you know something changed and you should at least check the changes.
 * To silence warnings, either bump the Actual value to match the new Expected once you have checked the API changes, or in the Engine ini hierarchy, set
 * 
 * [EOSShared]
 * bEnableApiVersionWarnings=false
 */
#if UE_WITH_EOS_SDK_APIVERSION_WARNINGS
#define UE_EOS_CHECK_API_MISMATCH(Actual, Expected) \
	struct PREPROCESSOR_JOIN(FEosApiMismatchMsg_, __LINE__) { \
		[[deprecated(#Actual " updated from " #Expected " to " PREPROCESSOR_TO_STRING(Actual) ", behaviour/params may have changed")]] \
		static constexpr int condition(TStaticDeprecateExpression<true>) { return 1; } \
		static constexpr int condition(TStaticDeprecateExpression<false>) { return 1; } \
	}; \
	enum class PREPROCESSOR_JOIN(EEosApiMismatchMsg_, __LINE__) { Value = PREPROCESSOR_JOIN(FEosApiMismatchMsg_, __LINE__)::condition(TStaticDeprecateExpression<Actual != Expected>()) }
#else
#define UE_EOS_CHECK_API_MISMATCH(Actual, Expected)
#endif

#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_base.h"
#include "eos_common.h"
#include "eos_version.h"

#if defined(DISABLE_EOSVOICECHAT_ENGINE)
#define WITH_EOS_RTC 0
#else
#define WITH_EOS_RTC WITH_EOS_SDK && (EOS_MAJOR_VERSION >= 1 && EOS_MINOR_VERSION >= 13)
#endif

#define EOS_ENUM_FORWARD_DECL(name) enum class name : int32_t;
EOS_ENUM_FORWARD_DECL(EOS_EApplicationStatus);
EOS_ENUM_FORWARD_DECL(EOS_EAuthScopeFlags);
EOS_ENUM_FORWARD_DECL(EOS_EAuthTokenType);
EOS_ENUM_FORWARD_DECL(EOS_EDesktopCrossplayStatus);
EOS_ENUM_FORWARD_DECL(EOS_EFriendsStatus);
EOS_ENUM_FORWARD_DECL(EOS_ELoginCredentialType);
EOS_ENUM_FORWARD_DECL(EOS_ENetworkStatus);
EOS_ENUM_FORWARD_DECL(EOS_Presence_EStatus);
#undef EOS_ENUM_FORWARD_DECL

DECLARE_LOG_CATEGORY_EXTERN(LogEOSSDK, Log, All);

EOSSHARED_API FString LexToString(const EOS_EResult EosResult);
EOSSHARED_API FString LexToString(const EOS_ProductUserId UserId);
EOSSHARED_API void LexFromString(EOS_ProductUserId& UserId, const TCHAR* String);
inline EOS_ProductUserId EOSProductUserIdFromString(const TCHAR* String)
{
	EOS_ProductUserId UserId;
	LexFromString(UserId, String);
	return UserId;
}

EOSSHARED_API FString LexToString(const EOS_EpicAccountId AccountId);
EOSSHARED_API void LexFromString(EOS_EpicAccountId& AccountId, const TCHAR* String);

EOSSHARED_API const TCHAR* LexToString(const EOS_EApplicationStatus ApplicationStatus);
EOSSHARED_API const TCHAR* LexToString(const EOS_EAuthTokenType AuthTokenType);
EOSSHARED_API const TCHAR* LexToString(const EOS_EDesktopCrossplayStatus DesktopCrossplayStatus);
EOSSHARED_API const TCHAR* LexToString(const EOS_EExternalAccountType ExternalAccountType);
EOSSHARED_API const TCHAR* LexToString(const EOS_EFriendsStatus FriendStatus);
EOSSHARED_API const TCHAR* LexToString(const EOS_ELoginStatus LoginStatus);
EOSSHARED_API const TCHAR* LexToString(const EOS_ENetworkStatus NetworkStatus);
EOSSHARED_API const TCHAR* LexToString(const EOS_Presence_EStatus PresenceStatus);

EOSSHARED_API bool LexFromString(EOS_EAuthScopeFlags& OutEnum, const FStringView InString);
EOSSHARED_API bool LexFromString(EOS_EExternalCredentialType& OutEnum, const TCHAR* InString);
EOSSHARED_API bool LexFromString(EOS_ELoginCredentialType& OutEnum, const TCHAR* InString);

#endif // WITH_EOS_SDK