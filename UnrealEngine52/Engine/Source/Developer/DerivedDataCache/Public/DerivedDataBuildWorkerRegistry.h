// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "DerivedDataSharedStringFwd.h"

struct FGuid;

namespace UE::DerivedData { class FBuildWorker; }
namespace UE::DerivedData { class IBuildWorkerExecutor; }

namespace UE::DerivedData
{

/**
 * A build worker registry maintains a collection of build workers.
 */
class IBuildWorkerRegistry
{
public:
	virtual ~IBuildWorkerRegistry() = default;

	/**
	 * Finds a build worker that can execute the function at the version.
	 *
	 * @param Function            The function to find a worker for.
	 * @param FunctionVersion     The version required for the function.
	 * @param BuildSystemVersion  The version required for the build system.
	 * @param OutWorkerExecutor   The executor to use to execute the worker.
	 * @return A build worker and executor if a compatible pair was found, or null for both.
	 */
	virtual FBuildWorker* FindWorker(
		const FUtf8SharedString& Function,
		const FGuid& FunctionVersion,
		const FGuid& BuildSystemVersion,
		IBuildWorkerExecutor*& OutWorkerExecutor) const = 0;
};

} // UE::DerivedData
