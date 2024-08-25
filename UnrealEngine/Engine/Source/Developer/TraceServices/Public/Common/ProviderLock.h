// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{

struct TRACESERVICES_API FProviderEditScopeLock
{
	FProviderEditScopeLock(const IEditableProvider& InProvider)
		: Provider(InProvider)
	{
		Provider.BeginEdit();
	}

	~FProviderEditScopeLock()
	{
		Provider.EndEdit();
	}

	const IEditableProvider& Provider;
};

struct TRACESERVICES_API FProviderReadScopeLock
{
	FProviderReadScopeLock(const IProvider& InProvider)
		: Provider(InProvider)
	{
		Provider.BeginRead();
	}

	~FProviderReadScopeLock()
	{
		Provider.EndRead();
	}

	const IProvider& Provider;
};

/**
 * Utility class to implement the read/write lock for a provider.
 * Example usage:
       extern thread_local FProviderLock::FThreadLocalState MyProviderLockState;
 *     virtual void EditAccessCheck() const override.{ Lock.WriteAccessCheck(MyProviderLockState); }
 *     FProviderLock Lock;
 */
class FProviderLock
{
public:
	struct TRACESERVICES_API FThreadLocalState
	{
		FProviderLock* Lock;
		int32 ReadLockCount;
		int32 WriteLockCount;
	};

public:
	TRACESERVICES_API void ReadAccessCheck(FThreadLocalState& State) const;
	TRACESERVICES_API void WriteAccessCheck(FThreadLocalState& State) const;

	TRACESERVICES_API void BeginRead(FThreadLocalState& State);
	TRACESERVICES_API void EndRead(FThreadLocalState& State);

	TRACESERVICES_API void BeginWrite(FThreadLocalState& State);
	TRACESERVICES_API void EndWrite(FThreadLocalState& State);

	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void ReadAccessCheck(const FProviderLock* CurrentProviderLock, const int32& CurrentReadProviderLockCount, const int32& CurrentWriteProviderLockCount) const;
	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void WriteAccessCheck(const int32& CurrentWriteProviderLockCount) const;

	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void BeginRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount, const int32& WriteProviderLockCount);
	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void EndRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount);

	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void BeginWrite(FProviderLock*& CurrentProviderLock, const int32& CurrentReadProviderLockCount, int32& WriteProviderLockCount);
	UE_DEPRECATED(5.3, "Please use the FThreadLocalState overload")
	void EndWrite(FProviderLock*& CurrentProviderLock, int32& WriteProviderLockCount);

private:
	FRWLock RWLock;
};

} // namespace TraceServices
