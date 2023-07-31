// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringView.h"
#include "DerivedDataSharedStringFwd.h"
#include "Memory/MemoryFwd.h"

class FCbObject;
class FCompressedBuffer;

namespace UE::DerivedData { class FBuildConfigContext; }
namespace UE::DerivedData { class FBuildContext; }
namespace UE::DerivedData { class FCacheBucket; }
namespace UE::DerivedData { class FValue; }
namespace UE::DerivedData { struct FValueId; }
namespace UE::DerivedData { enum class EBuildPolicy : uint32; }
namespace UE::DerivedData { enum class ECachePolicy : uint32; }

namespace UE::DerivedData
{

/**
 * A build function is referenced by a build definition as the way to build its inputs.
 *
 * Functions are expected to be pure and maintain no state. Inputs are provided through the build
 * context, and outputs are saved through the build context.
 *
 * Functions have a version which is used as a proxy for their code. Any code changes that affect
 * the behavior of a function must have a corresponding change in the function version.
 *
 * Functions must be registered by a build function factory to be available to the build system.
 *
 * @see FBuildContext
 * @see FBuildConfigContext
 * @see TBuildFunctionFactory
 */
class IBuildFunction
{
public:
	virtual ~IBuildFunction() = default;

	/** Returns the name of the build function, which identifies it in a build definition. */
	virtual const FUtf8SharedString& GetName() const = 0;

	/** Returns the version of the build function, which must change when the function changes. */
	virtual FGuid GetVersion() const = 0;

	/**
	 * Build the output for the input in the build context.
	 *
	 * The build is complete when the function returns, unless BeginAsyncBuild has been called on
	 * the context, in which case a call to EndAsyncBuild from any thread marks the end.
	 *
	 * The build is considered to be successful unless an error was logged during its execution.
	 */
	virtual void Build(FBuildContext& Context) const = 0;

	/**
	 * Configure the build based on its constants.
	 *
	 * The default configuration is to use the function name as the cache bucket, use the default
	 * cache policy, use the default build policy, and enable non-deterministic output checks.
	 */
	virtual void Configure(FBuildConfigContext& Context) const {}

	/**
	 * Cancel an asynchronous build.
	 *
	 * This function is only called when canceling a build that has called BeginAsyncBuild, which
	 * requires this to be implemented to return only after EndAsyncBuild has been called exactly
	 * once on the given context. Due to timing, this can be called after EndAsyncBuild, and must
	 * handle that case, but will never be called unless BeginAsyncBuild has been called.
	 */
	virtual void CancelAsyncBuild(FBuildContext& Context) const {}
};

/** A build context provides the inputs for a build function and saves its outputs. */
class FBuildContext
{
public:
	virtual ~FBuildContext() = default;

	/** Returns the name by which to identify this build for logging and profiling. */
	virtual const FSharedString& GetName() const = 0;

	/** Returns the constant with the matching key, or an object with no fields if not found. */
	virtual FCbObject FindConstant(FUtf8StringView Key) const = 0;

	/** Returns the input with the matching key, or a null buffer if not found. */
	virtual FSharedBuffer FindInput(FUtf8StringView Key) const = 0;

	/** Adds a value to the build output. Must have a non-null buffer and a unique ID. */
	virtual void AddValue(const FValueId& Id, const FValue& Value) = 0;
	virtual void AddValue(const FValueId& Id, const FCompressedBuffer& Buffer) = 0;
	virtual void AddValue(const FValueId& Id, const FCompositeBuffer& Buffer, uint64 BlockSize = 0) = 0;
	virtual void AddValue(const FValueId& Id, const FSharedBuffer& Buffer, uint64 BlockSize = 0) = 0;
	virtual void AddValue(const FValueId& Id, const FCbObject& Object) = 0;

	/** Adds the error to the build output. Must be deterministic and based only on inputs. */
	virtual void AddError(FStringView Message) = 0;

	/** Adds the warning to the build output. Must be deterministic and based only on inputs. */
	virtual void AddWarning(FStringView Message) = 0;

	/** Adds the message to the build output. Must be deterministic and based only on inputs. */
	virtual void AddMessage(FStringView Message) = 0;

	/** Overrides the cache policy mask used when writing this build in the cache. */
	virtual void SetCachePolicyMask(ECachePolicy Policy) = 0;

	/**
	 * Make this an asynchronous build by making the caller responsible for completing the build.
	 *
	 * This is an advanced feature that bypasses many of the safety checks for synchronous builds
	 * that validate that the build function only accesses inputs through the context. Take extra
	 * care to only consume inputs available in the build context and avoid reading global state.
	 *
	 * This may be called at most once on a build context. A function that uses this must support
	 * async cancellation by implementing CancelAsyncBuild. Once this has been called, the caller
	 * is responsible for calling EndAsyncBuild to finish this build. An async build may end from
	 * any thread, but the context is only safe to use from one thread at a time.
	 */
	virtual void BeginAsyncBuild() = 0;

	/**
	 * Mark the end of an asynchronous build.
	 *
	 * It is invalid to call any other function on the build context after calling this.
	 */
	virtual void EndAsyncBuild() = 0;
};

/** A build config context allows cache and build behavior to be modified based on constant inputs. */
class FBuildConfigContext
{
public:
	virtual ~FBuildConfigContext() = default;

	/** Returns the name by which to identify this build for logging and profiling. */
	virtual const FSharedString& GetName() const = 0;

	/** Returns the constant with the matching key, or an object with no fields if not found. */
	virtual FCbObject FindConstant(FUtf8StringView Key) const = 0;

	/** Adds the error to the build output. Must be deterministic and based only on inputs. */
	virtual void AddError(FStringView Message) = 0;

	/** Adds the warning to the build output. Must be deterministic and based only on inputs. */
	virtual void AddWarning(FStringView Message) = 0;

	/** Adds the message to the build output. Must be deterministic and based only on inputs. */
	virtual void AddMessage(FStringView Message) = 0;

	/** Overrides the cache bucket used when reading or writing this build in the cache. */
	virtual void SetCacheBucket(FCacheBucket Bucket) = 0;

	/** Returns the cache policy mask used when reading or writing this build in the cache. */
	virtual ECachePolicy GetCachePolicyMask() const = 0;

	/**
	 * Overrides the cache policy mask used when reading or writing this build in the cache.
	 *
	 * Allows Query and Store flags to be cleared for this build by removing them from the mask.
	 *
	 * Set the mask to the bitwise complement of the flags to be cleared: ~ECachePolicy::Remote.
	 */
	virtual void SetCachePolicyMask(ECachePolicy Policy) = 0;

	/** Returns the build policy mask used when executing this build. */
	virtual EBuildPolicy GetBuildPolicyMask() const = 0;

	/**
	 * Overrides the build policy mask used when executing this build.
	 *
	 * Allows Build flags to be cleared for this build by removing them from the mask.
	 *
	 * Set the mask to the bitwise complement of the flags to be cleared: ~EBuildPolicy::BuildRemote.
	 */
	virtual void SetBuildPolicyMask(EBuildPolicy Policy) = 0;

	/** Sets the estimated peak memory required to execute the build, excluding input size. */
	virtual void SetRequiredMemory(uint64 RequiredMemory) = 0;

	/** Skips verification that this function has deterministic output. */
	virtual void SkipDeterministicOutputCheck() = 0;
};

} // UE::DerivedData
