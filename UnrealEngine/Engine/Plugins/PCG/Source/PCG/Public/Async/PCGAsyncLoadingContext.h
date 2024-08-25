// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Engine/StreamableManager.h"
#include "UObject/SoftObjectPath.h"

struct FPCGContext;

/**
* Extension interface to add Async Loading support to any Context.
* Just inherit IPCGAsyncLoadingContext on the element context.
*/
struct PCG_API IPCGAsyncLoadingContext
{
public:
	virtual ~IPCGAsyncLoadingContext();

	/** Request a load. If load was already requested, do nothing. LoadHandle will be set in the context, meaning that assets will stay alive while context is loaded.
	* Request can be synchronous or asynchronous. If loading is asynchronous, the current task is paused and will be woken up when the loading is done.
	* WARNING: Make sure to call this function with soft paths that are NOT null.
	* Returns true if the execution can continue (objects are loaded or invalid), or false if we need to wait for loading
	*/
	bool RequestResourceLoad(FPCGContext* ThisContext, TArray<FSoftObjectPath>&& ObjectsToLoad, bool bAsynchronous = true);

	void CancelLoading();

	bool WasLoadRequested() const { return bLoadRequested; }

private:
	/** If the load was already requested */
	bool bLoadRequested = false;

	/** Handle holder for any loaded resources */
	TSharedPtr<FStreamableHandle> LoadHandle;
};