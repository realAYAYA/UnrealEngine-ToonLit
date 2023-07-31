// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DerivedDataBuildJob.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataRequest.h"
#include "HAL/Platform.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"

namespace UE::DerivedData { class IBuildJobSchedule; }
namespace UE::DerivedData { class IRequestOwner; }
namespace UE::DerivedData { struct FBuildSchedulerParams; }

namespace UE::DerivedData
{

/**
 * A build scheduler is responsible for deciding when and where a job executes in certain states.
 *
 * Jobs dispatch themselves to their scheduler when they are prepared to access limited resources
 * such as: memory, compute, storage, network. A scheduler may allow a job to execute immediately
 * or may queue it to execute later. A scheduler that uses a job queue is expected to execute the
 * jobs in priority order, respecting updates to priority.
 */
class IBuildScheduler
{
public:
	virtual ~IBuildScheduler() = default;

	/** Begin processing of the job by this scheduler. Always paired with IBuildJobSchedule::EndJob. */
	virtual TUniquePtr<IBuildJobSchedule> BeginJob(IBuildJob& Job, IRequestOwner& Owner) = 0;
};

/** Parameters that describe a build job to the build scheduler. */
struct FBuildSchedulerParams
{
	FBuildActionKey Key;

	/** Total size of constants and inputs, whether resolved or not. */
	uint64 TotalInputsSize = 0;
	/** Total size of constants and resolved inputs that are in memory now. */
	uint64 ResolvedInputsSize = 0;

	/** Total size of inputs that need to be resolved for local execution. Available in ResolveInputData. */
	uint64 MissingLocalInputsSize = 0;
	/** Total size of inputs that need to be resolved for remote execution. Available in ResolveInputData. */
	uint64 MissingRemoteInputsSize = 0;

	/** Estimate of the peak memory required to execute the build, including constants and inputs. */
	uint64 TotalRequiredMemory = 0;
};

/** Scheduling interface and context for a build job. */
class IBuildJobSchedule
{
public:
	virtual ~IBuildJobSchedule() = default;

	virtual FBuildSchedulerParams& EditParameters() = 0;

	/** Calls StepExecution() now or later. */
	virtual void ScheduleCacheQuery() = 0;
	/** Calls StepExecution() now or later. */
	virtual void ScheduleCacheStore() = 0;
	/** Calls StepExecution() now or later. */
	virtual void ScheduleResolveKey() = 0;
	/** Calls StepExecution() now or later. */
	virtual void ScheduleResolveInputMeta() = 0;
	/**
	 * Calls StepExecution() or SkipExecuteRemote() now or later.
	 *
	 * SkipExecuteRemote() won't be called unless MissingRemoteInputsSize is non-zero.
	 */
	virtual void ScheduleResolveInputData() = 0;
	/** Calls StepExecution() or SkipExecuteRemote() now or later. */
	virtual void ScheduleExecuteRemote() = 0;
	/** Calls StepExecution() now or later. */
	virtual void ScheduleExecuteLocal()	= 0;

	/** End processing of the job by this scheduler. Always paired with IBuildScheduler::BeginJob. */
	virtual void EndJob() = 0;
};

} // UE::DerivedData
