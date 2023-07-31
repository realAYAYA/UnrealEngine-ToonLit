// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "CoreTypes.h"
#include "DerivedDataRequestTypes.h"
#include "DerivedDataSharedStringFwd.h"

template <typename FuncType> class TUniqueFunction;

namespace UE::DerivedData { class FBuildOutput; }
namespace UE::DerivedData { class IBuild; }
namespace UE::DerivedData { class ICache; }
namespace UE::DerivedData { struct FBuildJobCompleteParams; }
namespace UE::DerivedData { struct FCacheKey; }
namespace UE::DerivedData { enum class EBuildStatus : uint32; }

namespace UE::DerivedData
{

using FOnBuildJobComplete = TUniqueFunction<void (FBuildJobCompleteParams&& Params)>;

/**
 * A build job is responsible for the execution of one build.
 *
 * Jobs typically proceed through each one of a sequence of states, though a state may be skipped
 * if the action was found in the cache or if the scheduler finds duplicate jobs for a definition
 * or an action.
 *
 * The job depends on the build scheduler to move it through its states. That relationship allows
 * the scheduler more control over resources such as: memory, compute, storage, network.
 */
class IBuildJob
{
public:
	/** Returns the name by which to identify this job for logging and profiling. */
	virtual const FSharedString& GetName() const = 0;
	/** Returns the name of the function to build with, or "Unknown" if not resolved yet. */
	virtual const FUtf8SharedString& GetFunction() const = 0;

	/** Returns the cache associated with this job. */
	virtual ICache& GetCache() const = 0;
	/** Returns the build system associated with this job. */
	virtual IBuild& GetBuild() const = 0;

	/** Called by the scheduler to continue this job on the calling thread. */
	virtual void StepExecution() = 0;

	/** Called by the scheduler to skip remote execution and fall back to local execution if permitted. */
	virtual void SkipExecuteRemote() = 0;

	/** Called by the scheduler if it has cached output compatible with the build policy. */
	virtual void SetOutput(const FBuildOutput& Output) = 0;
};

/** Parameters for the completion callback for build jobs. */
struct FBuildJobCompleteParams
{
	/** Job that is complete. */
	const IBuildJob& Job;
	/** Key for the job in the cache. Empty if the build completes before the key is assigned. */
	const FCacheKey& CacheKey;
	/** Output for the job that completed or was canceled. */
	FBuildOutput&& Output;
	/** Detailed status of the job. */
	EBuildStatus BuildStatus{};
	/** Basic status of the job. */
	EStatus Status = EStatus::Error;
};

} // UE::DerivedData
