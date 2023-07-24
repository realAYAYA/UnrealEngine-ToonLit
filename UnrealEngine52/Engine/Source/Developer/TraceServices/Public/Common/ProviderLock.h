// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{

struct TRACESERVICES_API FProviderEditScopeLock
{
	FProviderEditScopeLock(const IProvider& InProvider)
		: Provider(InProvider)
	{
		Provider.BeginEdit();
	}

	~FProviderEditScopeLock()
	{
		Provider.EndEdit();
	}

	const IProvider& Provider;
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

class FProviderLock
{
public:
	void ReadAccessCheck(const FProviderLock* CurrentProviderLock, const int32& CurrentReadProviderLockCount, const int32& CurrentWriteProviderLockCount) const;
	void WriteAccessCheck(const int32& CurrentWriteProviderLockCount) const;

	void BeginRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount, const int32& WriteProviderLockCount);
	void EndRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount);

	void BeginWrite(FProviderLock*& CurrentProviderLock, const int32& CurrentReadProviderLockCount, int32& WriteProviderLockCount);
	void EndWrite(FProviderLock*& CurrentProviderLock, int32& WriteProviderLockCount);

private:
	FRWLock RWLock;
};

} // namespace TraceServices
