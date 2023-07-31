// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DerivedDataSharedStringFwd.h"
#include "HAL/Platform.h"

#define UE_API DERIVEDDATACACHE_API

struct FGuid;

namespace UE::DerivedData { class FBuildActionBuilder; }
namespace UE::DerivedData { class FBuildDefinitionBuilder; }
namespace UE::DerivedData { class FBuildInputsBuilder; }
namespace UE::DerivedData { class FBuildOutputBuilder; }
namespace UE::DerivedData { class FBuildSession; }
namespace UE::DerivedData { class IBuildFunctionRegistry; }
namespace UE::DerivedData { class IBuildInputResolver; }
namespace UE::DerivedData { class IBuildScheduler; }
namespace UE::DerivedData { class IBuildWorkerRegistry; }

namespace UE::DerivedData { enum class EPriority : uint8; }

namespace UE::DerivedData
{

/**
 * Interface to the build system.
 *
 * Executing a build typically requires a definition, input resolver, session, and function.
 *
 * Use IBuild::CreateDefinition() to make a new build definition, or use IBuild::LoadDefinition()
 * to load a build definition that was previously saved. This references the function to execute,
 * and the inputs needed by the function.
 *
 * Use IBuild::CreateSession() to make a new build session with a build input resolver to resolve
 * input references into the referenced data. Use FBuildSession::Build() to schedule a definition
 * to build, along with any of its transitive build dependencies.
 *
 * Implement a IBuildFunction, with a unique name and version, to add values to the build context
 * based on constants and inputs in the context. Use TBuildFunctionFactory to add the function to
 * the registry at IBuild::GetFunctionRegistry() to allow the build job to find it.
 */
class IBuild
{
public:
	virtual ~IBuild() = default;

	/**
	 * Create a build definition builder.
	 *
	 * @param Name       The name by which to identify this definition for logging and profiling.
	 * @param Function   The name of the build function with which to build this definition.
	 */
	virtual FBuildDefinitionBuilder CreateDefinition(const FSharedString& Name, const FUtf8SharedString& Function) = 0;

	/**
	 * Create a build action builder.
	 *
	 * @param Name       The name by which to identify this action for logging and profiling.
	 * @param Function   The name of the build function that produced this action.
	 */
	virtual FBuildActionBuilder CreateAction(const FSharedString& Name, const FUtf8SharedString& Function) = 0;

	/**
	 * Create a build inputs builder.
	 *
	 * @param Name   The name by which to identify the inputs for logging and profiling.
	 */
	virtual FBuildInputsBuilder CreateInputs(const FSharedString& Name) = 0;

	/**
	 * Create a build output builder.
	 *
	 * @param Name       The name by which to identify this output for logging and profiling.
	 * @param Function   The name of the build function that produced this output.
	 */
	virtual FBuildOutputBuilder CreateOutput(const FSharedString& Name, const FUtf8SharedString& Function) = 0;

	/**
	 * Create a build session.
	 *
	 * An input resolver is required for the session to perform builds with unresolved inputs, or
	 * to resolve build value keys.
	 *
	 * A default scheduler is used if one is not provided. Using the default is recommended.
	 *
	 * @param Name            The name by which to identify this session for logging and profiling.
	 * @param InputResolver   The input resolver to resolve inputs for requested builds. Optional.
	 * @param Scheduler       The scheduler for builds created through the session. Optional.
	 */
	virtual FBuildSession CreateSession(
		const FSharedString& Name,
		IBuildInputResolver* InputResolver = nullptr,
		IBuildScheduler* Scheduler = nullptr) = 0;

	/**
	 * Returns the version of the build system.
	 *
	 * This version is expected to change very infrequently, only when formats and protocols used by
	 * the build system are changed in a way that breaks compatibility. This version is incorporated
	 * into build actions to keep the build output separate for different build versions.
	 */
	virtual const FGuid& GetVersion() const = 0;

	/**
	 * Returns the build function registry used by the build system.
	 */
	virtual IBuildFunctionRegistry& GetFunctionRegistry() const = 0;

	/**
	 * Returns the build worker registry used by the build system.
	 */
	virtual IBuildWorkerRegistry& GetWorkerRegistry() const = 0;
};

/** Returns a reference to the build system. Asserts if not available. */
UE_API IBuild& GetBuild();

} // UE::DerivedData

#undef UE_API
