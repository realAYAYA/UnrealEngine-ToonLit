// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "DerivedDataRequestTypes.h"
#include "DerivedDataValueId.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/RefCounting.h"

#define UE_API DERIVEDDATACACHE_API

namespace UE::DerivedData { class FBuildOutput; }
namespace UE::DerivedData { struct FBuildCompleteParams; }
namespace UE::DerivedData { struct FCacheKey; }
namespace UE::DerivedData::Private { class IBuildPolicyShared; }

namespace UE::DerivedData
{

using FOnBuildComplete = TUniqueFunction<void (FBuildCompleteParams&& Params)>;

/**
 * Flags to control the behavior of build requests.
 *
 * The build policy flags can be combined to support a variety of usage patterns. Examples:
 *
 * Build(Default): allow cache get; build if missing; return every value.
 * Build(Build | CacheStore): never query the cache; always build; return every value.
 * Build(Cache | CacheSkipData): allow cache get; never build; skip every value.
 *
 * BuildValue(Default): allow cache get; build if missing; return one value.
 * BuildValue(Build | CacheStore): never get from the cache; always build; return one value.
 * BuildValue(Cache | CacheSkipData): allow cache get; never build; skip every value.
 */
enum class EBuildPolicy : uint32
{
	/** A value without any flags set. */
	None                = 0,

	/** Allow local execution of the build function. */
	BuildLocal          = 1 << 0,
	/** Allow remote execution of the build function if it has a registered build worker. */
	BuildRemote         = 1 << 1,
	/** Allow local and remote execution of the build function. */
	Build               = BuildLocal | BuildRemote,

	/** Allow a cache query to avoid having to build. */
	CacheQuery          = 1 << 2,
	/** Allow a cache store to persist the build output when another cache store contains it. */
	CacheStoreOnQuery   = 1 << 3,
	/** Allow a cache store to persist the build output when the build function executes. */
	CacheStoreOnBuild   = 1 << 4,
	/** Allow a cache store to persist the build output. */
	CacheStore          = CacheStoreOnQuery | CacheStoreOnBuild,
	/** Allow a cache query and a cache store for the build. */
	Cache               = CacheQuery | CacheStore,

	/** Keep records in the cache for at least the duration of the session. */
	CacheKeepAlive      = 1 << 5,

	/** Skip fetching or returning data for the values. */
	SkipData            = 1 << 6,

	/** Allow cache query+store, allow local+remote build when missed or skipped, and fetch the value(s). */
	Default             = Build | Cache,
};

ENUM_CLASS_FLAGS(EBuildPolicy);

/** A value ID and the build policy to use for that value. */
struct FBuildValuePolicy
{
	FValueId Id;
	EBuildPolicy Policy = EBuildPolicy::Default;
};

/** Interface for the private implementation of the build policy. */
class Private::IBuildPolicyShared
{
public:
	virtual ~IBuildPolicyShared() = default;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
	virtual TConstArrayView<FBuildValuePolicy> GetValuePolicies() const = 0;
	virtual void AddValuePolicy(const FBuildValuePolicy& Policy) = 0;
};

/** Flags to control the behavior of build requests, with optional overrides by value. */
class FBuildPolicy
{
public:
	/** Construct a build policy that uses the default policy. */
	FBuildPolicy() = default;

	/** Construct a build policy with a single policy for every value. */
	inline FBuildPolicy(EBuildPolicy Policy)
		: CombinedPolicy(Policy)
		, DefaultPolicy(Policy)
	{
	}

	/** Returns true if every value uses the same build policy. */
	inline bool IsUniform() const { return !Shared; }

	/** Returns the build policy combined from the value policies. */
	inline EBuildPolicy GetCombinedPolicy() const { return CombinedPolicy; }

	/** Returns the build policy to use for values with no override. */
	inline EBuildPolicy GetDefaultPolicy() const { return DefaultPolicy; }

	/** Returns the build policy to use for the value. */
	UE_API EBuildPolicy GetValuePolicy(const FValueId& Id) const;

	/** Returns the array of build policy overrides for values, sorted by ID. */
	inline TConstArrayView<FBuildValuePolicy> GetValuePolicies() const
	{
		return Shared ? Shared->GetValuePolicies() : TConstArrayView<FBuildValuePolicy>();
	}

	/** Returns a copy of this policy transformed by an operation. */
	UE_API FBuildPolicy Transform(TFunctionRef<EBuildPolicy (EBuildPolicy)> Op) const;

private:
	friend class FBuildPolicyBuilder;

	EBuildPolicy CombinedPolicy = EBuildPolicy::Default;
	EBuildPolicy DefaultPolicy = EBuildPolicy::Default;
	TRefCountPtr<const Private::IBuildPolicyShared> Shared;
};

/** A build policy builder is used to construct a build policy. */
class FBuildPolicyBuilder
{
public:
	/** Construct a policy builder that uses the default policy. */
	FBuildPolicyBuilder() = default;

	/** Construct a policy builder that uses the provided policy for values with no override. */
	inline explicit FBuildPolicyBuilder(EBuildPolicy Policy)
		: BasePolicy(Policy)
	{
	}

	/** Adds a build policy override for a value. */
	UE_API void AddValuePolicy(const FBuildValuePolicy& Value);
	inline void AddValuePolicy(const FValueId& Id, EBuildPolicy Policy) { AddValuePolicy({Id, Policy}); }

	/** Build a build policy, which makes this builder subsequently unusable. */
	UE_API FBuildPolicy Build();

private:
	EBuildPolicy BasePolicy = EBuildPolicy::Default;
	TRefCountPtr<Private::IBuildPolicyShared> Shared;
};

/** Flags for build request completion callbacks. */
enum class EBuildStatus : uint32
{
	/** A value without any flags set. */
	None            = 0,

	/** The build function was executed locally. */
	BuildLocal      = 1 << 0,
	/** The build function was executed remotely. */
	BuildRemote     = 1 << 1,
	/** The build action and inputs were exported. */
	BuildExport     = 1 << 2,

	/** An attempt was made to execute the build function remotely. */
	BuildTryRemote  = 1 << 3,
	/** An attempt was made to export the build action and inputs. */
	BuildTryExport  = 1 << 4,

	/** The build made a cache query request. */
	CacheQuery      = 1 << 5,
	/** Valid build output was found in the cache. */
	CacheQueryHit   = 1 << 6,

	/** The build made a cache store request. */
	CacheStore      = 1 << 7,
	/** Valid build output was stored in the cache. */
	CacheStoreHit   = 1 << 8,

	/** The cache key was calculated. */
	CacheKey        = 1 << 9,
};

ENUM_CLASS_FLAGS(EBuildStatus);

/** Parameters for the completion callback for build requests. */
struct FBuildCompleteParams
{
	/** Key for the build in the cache. Empty if the build completes before the key is assigned. */
	const FCacheKey& CacheKey;

	/**
	 * Output for the build request that completed or was canceled.
	 *
	 * The name, function, and diagnostics are always populated.
	 *
	 * The values are populated when Status is Ok, but with null data if skipped by the policy.
	 */
	FBuildOutput&& Output;

	/** Detailed status of the build request. */
	EBuildStatus BuildStatus = EBuildStatus::None;

	/** Basic status of the build request. */
	EStatus Status = EStatus::Error;
};

} // UE::DerivedData

#undef UE_API
