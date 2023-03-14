// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataValueId.h"
#include "HAL/Platform.h"
#include "Memory/MemoryFwd.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

class FCbObject;
class FCbPackage;
class FCbWriter;

namespace UE::DerivedData { class FCacheRecord; }
namespace UE::DerivedData { class FOptionalCacheRecord; }
namespace UE::DerivedData { class FValue; }
namespace UE::DerivedData { class FValueWithId; }
namespace UE::DerivedData { class IRequestOwner; }
namespace UE::DerivedData { using FOnCacheRecordComplete = TUniqueFunction<void (FCacheRecord&& Record)>; }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::DerivedData::Private
{

class ICacheRecordInternal
{
public:
	virtual ~ICacheRecordInternal() = default;
	virtual const FCacheKey& GetKey() const = 0;
	virtual const FCbObject& GetMeta() const = 0;
	virtual const FValueWithId& GetValue(const FValueId& Id) const = 0;
	virtual TConstArrayView<FValueWithId> GetValues() const = 0;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
};

FCacheRecord CreateCacheRecord(ICacheRecordInternal* Record);

class ICacheRecordBuilderInternal
{
public:
	virtual ~ICacheRecordBuilderInternal() = default;
	virtual void SetMeta(FCbObject&& Meta) = 0;
	virtual void AddValue(const FValueId& Id, const FValue& Value) = 0;
	virtual FCacheRecord Build() = 0;
	virtual void BuildAsync(IRequestOwner& Owner, FOnCacheRecordComplete&& OnComplete) = 0;
};

} // UE::DerivedData::Private

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::DerivedData
{

/**
 * A cache record is a key, an array of values, and metadata.
 *
 * The key must uniquely correspond to its values. The key must never be reused for other values.
 * The metadata does not have this requirement and may be used to persist details that vary, such
 * as the time to generate the values or the machine that generated them.
 */
class FCacheRecord
{
public:
	/** Returns the key that identifies this record in the cache. */
	inline const FCacheKey& GetKey() const { return Record->GetKey(); }

	/** Returns the metadata. Null when requested with ECachePolicy::SkipMeta. */
	inline const FCbObject& GetMeta() const { return Record->GetMeta(); }

	/** Returns the value matching the ID. Null if no match. Data is null if skipped. */
	inline const FValueWithId& GetValue(const FValueId& Id) const { return Record->GetValue(Id); }

	/** Returns a view of the values ordered by ID. Data is null if skipped. */
	inline TConstArrayView<FValueWithId> GetValues() const { return Record->GetValues(); }

	/** Save the cache record to a compact binary package. */
	UE_API FCbPackage Save() const;
	/** Append the cache record to an existing package and writer for e.g. a batch of records. */
	UE_API void Save(FCbPackage& Attachments, FCbWriter& Writer) const;

	/** Load a cache record from a compact binary package. Null on error. */
	UE_API static FOptionalCacheRecord Load(const FCbPackage& Package);
	UE_API static FOptionalCacheRecord Load(const FCbPackage& Attachments, const FCbObject& Object);

private:
	friend class FOptionalCacheRecord;
	friend FCacheRecord Private::CreateCacheRecord(Private::ICacheRecordInternal* Record);

	/** Construct a cache record. Use Build() or BuildAsync() on FCacheRecordBuilder. */
	inline explicit FCacheRecord(Private::ICacheRecordInternal* InRecord)
		: Record(InRecord)
	{
	}

	TRefCountPtr<Private::ICacheRecordInternal> Record;
};

/**
 * A cache record builder is used to construct a cache record.
 *
 * Create using a key that uniquely corresponds to the values for the cache record.
 * The metadata may vary between records with the same key.
 *
 * @see FCacheRecord
 */
class FCacheRecordBuilder
{
public:
	/**
	 * Create a cache record builder from a cache key.
	 */
	UE_API explicit FCacheRecordBuilder(const FCacheKey& Key);

	/**
	 * Set the metadata for the cache record.
	 *
	 * @param Meta   The metadata, which is cloned if not owned.
	 */
	inline void SetMeta(FCbObject&& Meta)
	{
		return RecordBuilder->SetMeta(MoveTemp(Meta));
	}

	/**
	 * Add a value to the cache record.
	 *
	 * @param Id          An ID for the value that is unique within this cache record.
	 * @param Buffer      The value, which is compressed by the builder, and cloned if not owned.
	 * @param BlockSize   The power-of-two block size to encode raw data in. 0 is default.
	 */
	UE_API void AddValue(const FValueId& Id, const FCompositeBuffer& Buffer, uint64 BlockSize = 0);
	UE_API void AddValue(const FValueId& Id, const FSharedBuffer& Buffer, uint64 BlockSize = 0);

	/**
	 * Add a value to the cache record.
	 *
	 * @note The ID for the value must be unique within this cache record.
	 */
	UE_API void AddValue(const FValueWithId& Value);

	/**
	 * Add a value to the cache record.
	 *
	 * @param Id   An ID for the value that is unique within this cache record.
	 */
	inline void AddValue(const FValueId& Id, const FValue& Value)
	{
		return RecordBuilder->AddValue(Id, Value);
	}

	/**
	 * Build a cache record, which makes this builder subsequently unusable.
	 *
	 * Prefer BuildAsync() when the values are added from raw buffers, which requires compressing
	 * those buffers before constructing the cache record.
	 */
	inline FCacheRecord Build()
	{
		ON_SCOPE_EXIT { RecordBuilder = nullptr; };
		return RecordBuilder->Build();
	}

	/**
	 * Build a cache record asynchronously, which makes this builder subsequently unusable.
	 *
	 * Prefer Build() when the values are added from compressed buffers as compression is already
	 * complete and BuildAsync() will complete immediately in that case.
	 */
	inline void BuildAsync(IRequestOwner& Owner, FOnCacheRecordComplete&& OnComplete)
	{
		return RecordBuilder.Release()->BuildAsync(Owner, MoveTemp(OnComplete));
	}

private:
	TUniquePtr<Private::ICacheRecordBuilderInternal> RecordBuilder;
};

/**
 * A cache record that can be null.
 *
 * @see FCacheRecord
 */
class FOptionalCacheRecord : private FCacheRecord
{
public:
	inline FOptionalCacheRecord() : FCacheRecord(nullptr) {}

	inline FOptionalCacheRecord(FCacheRecord&& InRecord) : FCacheRecord(MoveTemp(InRecord)) {}
	inline FOptionalCacheRecord(const FCacheRecord& InRecord) : FCacheRecord(InRecord) {}
	inline FOptionalCacheRecord& operator=(FCacheRecord&& InRecord) { FCacheRecord::operator=(MoveTemp(InRecord)); return *this; }
	inline FOptionalCacheRecord& operator=(const FCacheRecord& InRecord) { FCacheRecord::operator=(InRecord); return *this; }

	/** Returns the cache record. The caller must check for null before using this accessor. */
	inline const FCacheRecord& Get() const & { return *this; }
	inline FCacheRecord Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Record.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalCacheRecord(); }
};

} // UE::DerivedData

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::DerivedData
{

/** An implementation of KeyFuncs to compare FCacheRecord by its FCacheKey. */
struct FCacheRecordKeyFuncs
{
	using KeyType = FCacheKey;
	using KeyInitType = const FCacheKey&;
	using ElementInitType = const FCacheRecord&;

	static constexpr bool bAllowDuplicateKeys = false;

	static inline KeyInitType GetSetKey(ElementInitType Record) { return Record.GetKey(); }
	static inline uint32 GetKeyHash(KeyInitType Key) { return GetTypeHash(Key); }
	static inline bool Matches(KeyInitType A, KeyInitType B) { return A == B; }
};

} // UE::DerivedData

#undef UE_API
