// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/ProviderLock.h"

namespace TraceServices
{

void FProviderLock::ReadAccessCheck(const FProviderLock* CurrentProviderLock, const int32& CurrentReadProviderLockCount, const int32& CurrentWriteProviderLockCount) const
{
	checkf(CurrentProviderLock == this && (CurrentReadProviderLockCount > 0 || CurrentWriteProviderLockCount > 0),
		TEXT("Trying to READ from provider outside of a READ scope"));
}

void FProviderLock::WriteAccessCheck(const int32& CurrentWriteAllocationsProviderLockCount) const
{
	checkf(CurrentWriteAllocationsProviderLockCount > 0,
		TEXT("Trying to WRITE to provider outside of an EDIT/WRITE scope"));
}

void FProviderLock::BeginRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount, const int32& WriteProviderLockCount)
{
	checkf(WriteProviderLockCount == 0, TEXT("Trying to lock provider for READ while holding EDIT/WRITE access"));
	if (CurrentReadProviderLockCount++ == 0)
	{
		CurrentProviderLock = this;
		RWLock.ReadLock();
	}
}

void FProviderLock::EndRead(FProviderLock*& CurrentProviderLock, int32& CurrentReadProviderLockCount)
{
	check(CurrentReadProviderLockCount > 0);
	if (--CurrentReadProviderLockCount == 0)
	{
		RWLock.ReadUnlock();
		CurrentProviderLock = nullptr;
	}
}

void FProviderLock::BeginWrite(FProviderLock*& CurrentProviderLock, const int32& CurrentReadProviderLockCount, int32& WriteProviderLockCount)
{
	check(!CurrentProviderLock || CurrentProviderLock == this);
	checkf(CurrentReadProviderLockCount == 0, TEXT("Trying to lock provider for EDIT/WRITE while holding READ access"));
	if (WriteProviderLockCount++ == 0)
	{
		CurrentProviderLock = this;
		RWLock.WriteLock();
	}
}

void FProviderLock::EndWrite(FProviderLock*& CurrentProviderLock, int32& WriteProviderLockCount)
{
	check(WriteProviderLockCount > 0);
	if (--WriteProviderLockCount == 0)
	{
		RWLock.WriteUnlock();
		CurrentProviderLock = nullptr;
	}
}

} // namespace TraceServices
