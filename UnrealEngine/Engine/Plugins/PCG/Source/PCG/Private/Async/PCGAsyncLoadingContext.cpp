// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/PCGAsyncLoadingContext.h"

#include "PCGContext.h"

#include "Engine/AssetManager.h"

IPCGAsyncLoadingContext::~IPCGAsyncLoadingContext()
{
	// We need to make sure to cancel any loading requested.
	CancelLoading();
}

void IPCGAsyncLoadingContext::CancelLoading()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive())
	{
		LoadHandle->CancelHandle();
	}

	LoadHandle.Reset();
}

bool IPCGAsyncLoadingContext::RequestResourceLoad(FPCGContext* ThisContext, TArray<FSoftObjectPath>&& ObjectsToLoad, bool bAsynchronous)
{
	if (!ObjectsToLoad.IsEmpty() && !bLoadRequested)
	{
		if (!bAsynchronous)
		{
			LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(std::forward<TArray<FSoftObjectPath>>(ObjectsToLoad));
			bLoadRequested = true;

			return true;
		}
		else
		{
			ThisContext->bIsPaused = true;

			// It is a bit unsafe to pass this to a delegate lambda. But if the context dies before the completion of the loading, the context will cancel the loading in its dtor.
			LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(std::forward<TArray<FSoftObjectPath>>(ObjectsToLoad), [ThisContext]() { ThisContext->bIsPaused = false; });
			bLoadRequested = true;

			// If the load handle is not active it means objects were invalid
			if (!LoadHandle->IsActive())
			{
				ThisContext->bIsPaused = false;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}