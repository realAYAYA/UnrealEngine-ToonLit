// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_LOW_LEVEL_TESTS

#include "DerivedDataLegacyCacheStore.h"
#include "Misc/EnumClassFlags.h"

namespace UE::DerivedData
{

class ITestCacheStore : public ILegacyCacheStore
{
public:
	virtual ECacheStoreFlags GetFlags() const = 0;
	virtual uint32 GetTotalRequestCount() const = 0;
	virtual uint32 GetCanceledRequestCount() const = 0;

	virtual void AddRecord(const FCacheKey& Key, TConstArrayView<FValueWithId> Values, const FCbObject* Meta = nullptr) = 0;
	virtual TConstArrayView<FValueWithId> FindRecord(const FCacheKey& Key, FCbObject* OutMeta = nullptr) const = 0;

	virtual void AddValue(const FCacheKey& Key, const FValue& Value) = 0;
	virtual FValue FindValue(const FCacheKey& Key) const = 0;

	virtual void AddContent(const FCompressedBuffer& Content) = 0;
	virtual FCompressedBuffer FindContent(const FIoHash& RawHash, uint64 RawSize) const = 0;

	virtual TConstArrayView<FCachePutRequest> GetPutRequests() const = 0;
	virtual TConstArrayView<FCacheGetRequest> GetGetRequests() const = 0;
	virtual TConstArrayView<FCachePutValueRequest> GetPutValueRequests() const = 0;
	virtual TConstArrayView<FCacheGetValueRequest> GetGetValueRequests() const = 0;
	virtual TConstArrayView<FCacheGetChunkRequest> GetGetChunkRequests() const = 0;

	virtual void ExecuteAsync() = 0;
};

enum class ETestCacheStoreFlags : uint32
{
	None            = 0,
	/** Async execution of requests. */
	Async           = 1 << 0,
	/** Waits on async execution of requests. */
	Wait            = 1 << 1,
};

ENUM_CLASS_FLAGS(ETestCacheStoreFlags);

ITestCacheStore* CreateTestCacheStore(ECacheStoreFlags Flags, ETestCacheStoreFlags TestFlags);

} // UE::DerivedData

#endif // WITH_LOW_LEVEL_TESTS
