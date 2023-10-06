// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterServer.h"
#include "Network/DisplayClusterNetworkTypes.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "GenericPlatform/GenericPlatformAffinity.h"


class FSocket;
struct FIPv4Endpoint;
struct FDisplayClusterSessionInfo;


/**
 * Abstract DisplayCluster service
 */
class FDisplayClusterService
	: public FDisplayClusterServer
{
public:
	FDisplayClusterService(const FString& Name);

public:
	static EThreadPriority ConvertThreadPriorityFromCvarValue(int ThreadPriority);
	static EThreadPriority GetThreadPriority();
	static EDisplayClusterCommResult TranslateBarrierWaitResultIntoCommResult(EDisplayClusterBarrierWaitResult WaitResult);

protected:
	// Cache session info data if needed for child service implementations
	void SetSessionInfoCache(const FDisplayClusterSessionInfo& SessionInfo);
	const FDisplayClusterSessionInfo& GetSessionInfoCache() const;
	void ClearCache();

private:
	// Session info cache
	TMap<uint32, FDisplayClusterSessionInfo> SessionInfoCache;
	mutable FCriticalSection SessionInfoCacheCS;
};
