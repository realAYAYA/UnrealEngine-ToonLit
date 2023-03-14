// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "OnlineError.h"
#define LOCTEXT_NAMESPACE "OnlineFriend"
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.oss.friend"

namespace OnlineFriend
{
	#include "OnlineErrorMacros.inl"

	namespace Errors
	{
		inline FOnlineError InvalidResult() { return ONLINE_ERROR(EOnlineErrorResult::InvalidResults, TEXT("invalid_result")); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef ONLINE_ERROR_NAMESPACE




