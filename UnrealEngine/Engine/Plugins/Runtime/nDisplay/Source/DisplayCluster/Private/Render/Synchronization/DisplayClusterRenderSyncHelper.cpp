// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncHelper.h"

#include "Misc/ScopeLock.h"


// Singleton instance
TUniquePtr<IDisplayClusterRenderSyncHelper> FDisplayClusterRenderSyncHelper::Instance;

// Singleton access mutex
FCriticalSection FDisplayClusterRenderSyncHelper::CritSecInternals;


IDisplayClusterRenderSyncHelper& FDisplayClusterRenderSyncHelper::Get()
{
	if (!Instance)
	{
		FScopeLock Lock(&FDisplayClusterRenderSyncHelper::CritSecInternals);
		if (!Instance)
		{
			Instance = CreateHelper();
		}
	}

	return *Instance.Get();
}
