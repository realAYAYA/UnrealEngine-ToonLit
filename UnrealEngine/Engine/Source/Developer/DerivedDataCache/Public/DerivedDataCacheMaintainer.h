// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "UObject/NameTypes.h"

namespace UE::DerivedData
{

/**
 * A cache store may register an instance of this interface to take part in cache maintenance.
 *
 * As an example, a filesystem cache might register a maintainer that scans for and deletes stale
 * files within its cache directory. Boosting the priority removes any sleep between file scans.
 *
 * An example consumer is the DDCCleanup commandlet which uses this to boost priority and wait on
 * the completion of maintenance by cache stores.
 */
class ICacheStoreMaintainer : public IModularFeature
{
public:
	static inline const FLazyName FeatureName{"CacheStoreMaintainer"};

	/**
	 * True when maintenance is not active.
	 *
	 * This must return true eventually because it is called to wait for maintenance.
	 */
	virtual bool IsIdle() const = 0;

	/**
	 * Boost the priority of the active maintenance operation.
	 *
	 * This is expected to remove any delays, or otherwise allow the active maintenance operation
	 * to complete more quickly. This is called before waiting for maintenance to be idle.
	 */
	virtual void BoostPriority()
	{
	}
};

} // UE::DerivedData
