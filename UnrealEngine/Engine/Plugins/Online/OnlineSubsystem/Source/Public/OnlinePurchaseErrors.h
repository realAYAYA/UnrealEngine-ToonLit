// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineError.h"
#define LOCTEXT_NAMESPACE "OnlinePurchase"
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.oss.purchase"

namespace OnlinePurchase
{
	#include "OnlineErrorMacros.inl"

	namespace Errors
	{
		inline FOnlineError InvalidResult() { return ONLINE_ERROR(EOnlineErrorResult::InvalidResults, TEXT("invalid_result")); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef ONLINE_ERROR_NAMESPACE




