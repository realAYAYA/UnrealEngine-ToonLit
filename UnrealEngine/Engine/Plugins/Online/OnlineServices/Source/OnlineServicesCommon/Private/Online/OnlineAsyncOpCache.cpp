// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineAsyncOpCache.h"
#include "Online/OnlineExecHandler.h" // IWYU pragma: keep
#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

TSharedRef<FOnlineAsyncOpCache> FOnlineAsyncOpCache::GetSharedThis()
{
	return TSharedRef<FOnlineAsyncOpCache>(Services.AsShared(), this);
}

/* UE::Online*/ }
