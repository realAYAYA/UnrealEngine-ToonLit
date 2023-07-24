// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"

class FDerivedDataCacheInterface;

/**
 * Module for the Derived Data Cache and Derived Data Build.
 */
class IDerivedDataCacheModule : public IModuleInterface
{
public:
	/** Return the DDC interface **/
	UE_DEPRECATED(4.27, "GetDDC has been replaced by CreateOrGetCache.")
	virtual FDerivedDataCacheInterface& GetDDC() = 0;

	/**
	 * Returns the cache, which is created by the first call to this function.
	 *
	 * This always returns a pointer to a valid cache, but that pointer becomes null when the module
	 * shuts down and destroys the cache. This extra level of indirection allows a caller to observe
	 * the destruction of the cache without polling this function or monitoring the module lifetime.
	 */
	virtual FDerivedDataCacheInterface* const* CreateOrGetCache() = 0;

	/**
	 * Returns a pointer to the cache, which may be null.
	 *
	 * This returns a non-null pointer to a cache which may be null and becomes null when the module
	 * shuts down and destroys the cache. This extra level of indirection allows a caller to observe
	 * the destruction of the cache without polling this function or monitoring the module lifetime.
	 */
	virtual FDerivedDataCacheInterface* const* GetCache() = 0;
};
