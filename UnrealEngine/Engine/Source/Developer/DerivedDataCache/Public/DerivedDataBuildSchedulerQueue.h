// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DerivedDataSharedStringFwd.h"
#include "Features/IModularFeature.h"
#include "UObject/NameTypes.h"

template <typename FuncType> class TUniqueFunction;

namespace UE::DerivedData { class IRequestOwner; }

namespace UE::DerivedData
{

/**
 * Allows execution of a build to be queued in a type-specific way.
 *
 * At most one instance of this feature may be registered for each type name.
 * Builds are queued before they query the cache, or equivalent if the cache query is skipped.
 * The completion callback will capture the FExecutionResourceContext until the end of the build.
 */
class IBuildSchedulerTypeQueue : public IModularFeature
{
public:
	static inline const FLazyName FeatureName{"BuildSchedulerTypeQueue"};

	virtual ~IBuildSchedulerTypeQueue() = default;

	/** Returns the type name that this provider corresponds to. */
	virtual const FUtf8SharedString& GetTypeName() const = 0;

	virtual void Queue(IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete) = 0;
};

/**
 * Allows execution of a build to be queued until there is sufficient memory available.
 *
 * At most one instance of this feature may be registered.
 * Builds are queued before they load input data for local execution of the build.
 * The completion callback will capture the FExecutionResourceContext until the end of the build.
 */
class IBuildSchedulerMemoryQueue : public IModularFeature
{
public:
	static inline const FLazyName FeatureName{"BuildSchedulerMemoryQueue"};

	virtual ~IBuildSchedulerMemoryQueue() = default;

	virtual void Reserve(uint64 Memory, IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete) = 0;
};

} // UE::DerivedData
