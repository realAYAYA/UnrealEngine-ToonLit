// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "OnlineError.h"
#define LOCTEXT_NAMESPACE "OnlineIdentity"
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.oss.identity"

namespace OnlineIdentity
{
	#include "OnlineErrorMacros.inl"

	namespace Errors
	{
		inline FOnlineError InvalidCreds() { return ONLINE_ERROR(EOnlineErrorResult::InvalidCreds); }
		inline FOnlineError InvalidAuth() { return ONLINE_ERROR(EOnlineErrorResult::InvalidAuth); }

		inline FOnlineError UserNotFound() { return ONLINE_ERROR(EOnlineErrorResult::InvalidUser, TEXT("user_not_found")); }

		inline FOnlineError LoginPending() { return ONLINE_ERROR(EOnlineErrorResult::AlreadyPending, TEXT("login_pending")); }

		inline FOnlineError InvalidResult() { return ONLINE_ERROR(EOnlineErrorResult::InvalidResults, TEXT("invalid_result")); }

		inline FOnlineError PinGrantFailure() { return ONLINE_ERROR(EOnlineErrorResult::FailExtended, TEXT("pin_grant_failure")); }
		inline FOnlineError PinGrantTimeout() { return ONLINE_ERROR(EOnlineErrorResult::FailExtended, TEXT("pin_grant_timeout")); }

		// Params
		extern ONLINESUBSYSTEM_API const FString AuthLoginParam;
		extern ONLINESUBSYSTEM_API const FString AuthTypeParam;
		extern ONLINESUBSYSTEM_API const FString AuthPasswordParam;

		// Results
		extern ONLINESUBSYSTEM_API const FString NoUserId;
		extern ONLINESUBSYSTEM_API const FString NoAuthToken;
		extern ONLINESUBSYSTEM_API const FString NoAuthType;
	}
}


#undef LOCTEXT_NAMESPACE
#undef ONLINE_ERROR_NAMESPACE




