// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Containers/StringFwd.h"
#include "CoreTypes.h"
#include "DerivedDataValueId.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/RefCounting.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

class FCbFieldView;
class FCbObjectView;
class FCbWriter;
template <typename FuncType> class TFunctionRef;

namespace UE::DerivedData::Private { class ICacheRecordPolicyShared; }
namespace UE::DerivedData { class FOptionalCacheRecordPolicy; }

namespace UE::DerivedData
{

/**
 * Flags to control the behavior of cache requests.
 *
 * Flags can be combined to cover a variety of requirements. Examples:
 *
 * Get(Default): Fetch from any cache. Store the response to any caches if missing.
 * Get(Local): Fetch from any local cache. Store the response to any local caches if missing.
 * Get(Query | StoreLocal): Fetch from any cache. Store response to any local caches if missing.
 * Get(Query | SkipData): Check for existence in any cache. Do not store to any caches if missing.
 * Get(Default | SkipData): Check for existence in any cache. Store the response to any caches if missing.
 * Get(Default | PartialRecord): Fetch from any cache, and return a partial record if values are missing data.
 *
 * Put(Default): Store to every cache, and do not overwrite existing valid records or values.
 * Put(Store): Store to every cache, and overwrite existing records or values.
 * Put(Local): Store to every local cache, skipping remote caches.
 * Put(Default | PartialRecord): Store to every cache, even if the record has missing data for its values.
 */
enum class ECachePolicy : uint32
{
	/** A value with no flags. Disables access to the cache unless combined with other flags. */
	None            = 0,

	/** Allow a cache request to query local caches. */
	QueryLocal      = 1 << 0,
	/** Allow a cache request to query remote caches. */
	QueryRemote     = 1 << 1,
	/** Allow a cache request to query any caches. */
	Query           = QueryLocal | QueryRemote,

	/** Allow cache records and values to be stored in local caches. */
	StoreLocal      = 1 << 2,
	/** Allow cache records and values to be stored in remote caches. */
	StoreRemote     = 1 << 3,
	/** Allow cache records and values to be stored in any caches. */
	Store           = StoreLocal | StoreRemote,

	/** Allow cache requests to query and store records and values in local caches. */
	Local           = QueryLocal | StoreLocal,
	/** Allow cache requests to query and store records and values in remote caches. */
	Remote          = QueryRemote | StoreRemote,

	/** Allow cache requests to query and store records and values in any caches. */
	Default         = Query | Store,

	/** Skip fetching the data for values. */
	SkipData        = 1 << 4,

	/** Skip fetching the metadata for record requests. */
	SkipMeta        = 1 << 5,

	/**
	 * Partial output will be provided with the error status when a required value is missing.
	 *
	 * This is meant for cases when the missing values can be individually recovered, or rebuilt,
	 * without rebuilding the whole record. The cache automatically adds this flag when there are
	 * other cache stores that it may be able to recover missing values from.
	 *
	 * Missing values will be returned in the records, but with only the hash and size.
	 *
	 * Applying this flag for a put of a record allows a partial record to be stored.
	 */
	PartialRecord   = 1 << 6,

	/**
	 * Keep records and values in the cache for at least the duration of the session.
	 *
	 * This flag hints that the records and values that it is applied to may be accessed again in
	 * this session. The cache will make an effort to prevent eviction of the records and values,
	 * though an absolute guarantee is impossible.
	 *
	 * This flag is meant to be used when subsequent accesses will not tolerate a cache miss.
	 */
	KeepAlive       = 1 << 7,
};

ENUM_CLASS_FLAGS(ECachePolicy);

/** Append a non-empty text version of the policy to the builder. */
UE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, ECachePolicy Policy);
UE_API FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, ECachePolicy Policy);
UE_API FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, ECachePolicy Policy);

/** Try to parse a policy from text written by operator<<. */
UE_API bool TryLexFromString(ECachePolicy& OutPolicy, FUtf8StringView String);
UE_API bool TryLexFromString(ECachePolicy& OutPolicy, FWideStringView String);

UE_DEPRECATED(5.1, "Replace ParseCachePolicy with TryLexFromString.") UE_API ECachePolicy ParseCachePolicy(FAnsiStringView Text);
UE_DEPRECATED(5.1, "Replace ParseCachePolicy with TryLexFromString.") UE_API ECachePolicy ParseCachePolicy(FWideStringView Text);
UE_DEPRECATED(5.1, "Replace ParseCachePolicy with TryLexFromString.") UE_API ECachePolicy ParseCachePolicy(FUtf8StringView Text);

UE_API FCbWriter& operator<<(FCbWriter& Writer, ECachePolicy Policy);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, ECachePolicy& OutPolicy, ECachePolicy Default = ECachePolicy::Default);

/** A value ID and the cache policy to use for that value. */
struct FCacheValuePolicy
{
	FValueId Id;
	ECachePolicy Policy = ECachePolicy::Default;

	/** Flags that are valid on a value policy. */
	static constexpr ECachePolicy PolicyMask = ECachePolicy::Default | ECachePolicy::SkipData;
};

UE_API FCbWriter& operator<<(FCbWriter& Writer, const FCacheValuePolicy& Policy);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, FCacheValuePolicy& OutPolicy);

/** Interface for the private implementation of the cache record policy. */
class Private::ICacheRecordPolicyShared
{
public:
	virtual ~ICacheRecordPolicyShared() = default;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
	virtual void AddValuePolicy(const FCacheValuePolicy& Policy) = 0;
	virtual TConstArrayView<FCacheValuePolicy> GetValuePolicies() const = 0;
};

/**
 * Flags to control the behavior of cache record requests, with optional overrides by value.
 *
 * Examples:
 * - A base policy of None with value policy overrides of Default will fetch those values if they
 *   exist in the record, and skip data for any other values.
 * - A base policy of Default, with value policy overrides of (Query | SkipData), will skip those
 *   values, but still check if they exist, and will load any other values.
 */
class FCacheRecordPolicy
{
public:
	/** Construct a cache record policy that uses the default policy. */
	FCacheRecordPolicy() = default;

	/** Construct a cache record policy with a uniform policy for the record and every value. */
	inline FCacheRecordPolicy(ECachePolicy BasePolicy)
		: RecordPolicy(BasePolicy)
		, DefaultValuePolicy(BasePolicy & FCacheValuePolicy::PolicyMask)
	{
	}

	/** Returns true if this is the default cache policy with no overrides for values. */
	inline bool IsDefault() const { return !Shared && RecordPolicy == ECachePolicy::Default; }

	/** Returns true if the record and every value use the same cache policy. */
	inline bool IsUniform() const { return !Shared; }

	/** Returns the cache policy to use for the record. */
	inline ECachePolicy GetRecordPolicy() const { return RecordPolicy; }

	/** Returns the base cache policy that this was constructed from. */
	inline ECachePolicy GetBasePolicy() const
	{
		return DefaultValuePolicy | (RecordPolicy & ~FCacheValuePolicy::PolicyMask);
	}

	/** Returns the cache policy to use for the value. */
	UE_API ECachePolicy GetValuePolicy(const FValueId& Id) const;

	/** Returns the array of cache policy overrides for values, sorted by ID. */
	inline TConstArrayView<FCacheValuePolicy> GetValuePolicies() const
	{
		return Shared ? Shared->GetValuePolicies() : TConstArrayView<FCacheValuePolicy>();
	}

	/** Returns a copy of this policy transformed by an operation. */
	UE_API FCacheRecordPolicy Transform(TFunctionRef<ECachePolicy (ECachePolicy)> Op) const;

	/** Saves the cache record policy to a compact binary object. */
	UE_DEPRECATED(5.1, "Replace Policy.Save(Writer) with Writer << Policy.")
	UE_API void Save(FCbWriter& Writer) const;

	/** Loads a cache record policy from an object. */
	UE_DEPRECATED(5.1, "Replace Load(Object) with LoadFromCompactBinary(Field, Policy).")
	UE_API static FOptionalCacheRecordPolicy Load(FCbObjectView Object);

private:
	friend class FCacheRecordPolicyBuilder;
	friend class FOptionalCacheRecordPolicy;

	ECachePolicy RecordPolicy = ECachePolicy::Default;
	ECachePolicy DefaultValuePolicy = ECachePolicy::Default;
	TRefCountPtr<const Private::ICacheRecordPolicyShared> Shared;
};

/** A cache record policy builder is used to construct a cache record policy. */
class FCacheRecordPolicyBuilder
{
public:
	/** Construct a policy builder that uses the default policy as its base policy. */
	FCacheRecordPolicyBuilder() = default;

	/** Construct a policy builder that uses the provided policy for the record and values with no override. */
	inline explicit FCacheRecordPolicyBuilder(ECachePolicy Policy)
		: BasePolicy(Policy)
	{
	}

	/** Adds a cache policy override for a value. Must contain only flags in FCacheRecordValue::PolicyMask. */
	UE_API void AddValuePolicy(const FCacheValuePolicy& Value);
	inline void AddValuePolicy(const FValueId& Id, ECachePolicy Policy) { AddValuePolicy({Id, Policy}); }

	/** Build a cache record policy, which makes this builder subsequently unusable. */
	UE_API FCacheRecordPolicy Build();

private:
	ECachePolicy BasePolicy = ECachePolicy::Default;
	TRefCountPtr<Private::ICacheRecordPolicyShared> Shared;
};

/**
 * A cache record policy that can be null.
 *
 * @see FCacheRecordPolicy
 */
class FOptionalCacheRecordPolicy : private FCacheRecordPolicy
{
public:
	inline FOptionalCacheRecordPolicy() : FCacheRecordPolicy(~ECachePolicy::None) {}

	inline FOptionalCacheRecordPolicy(FCacheRecordPolicy&& InOutput) : FCacheRecordPolicy(MoveTemp(InOutput)) {}
	inline FOptionalCacheRecordPolicy(const FCacheRecordPolicy& InOutput) : FCacheRecordPolicy(InOutput) {}
	inline FOptionalCacheRecordPolicy& operator=(FCacheRecordPolicy&& InOutput) { FCacheRecordPolicy::operator=(MoveTemp(InOutput)); return *this; }
	inline FOptionalCacheRecordPolicy& operator=(const FCacheRecordPolicy& InOutput) { FCacheRecordPolicy::operator=(InOutput); return *this; }

	/** Returns the cache record policy. The caller must check for null before using this accessor. */
	inline const FCacheRecordPolicy& Get() const & { return *this; }
	inline FCacheRecordPolicy Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return RecordPolicy == ~ECachePolicy::None; }
	inline bool IsValid() const { return !IsNull(); }
	inline explicit operator bool() const { return !IsNull(); }

	inline void Reset() { *this = FOptionalCacheRecordPolicy(); }
};

UE_API FCbWriter& operator<<(FCbWriter& Writer, const FCacheRecordPolicy& Policy);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, FCacheRecordPolicy& OutPolicy);

} // UE::DerivedData

#undef UE_API
