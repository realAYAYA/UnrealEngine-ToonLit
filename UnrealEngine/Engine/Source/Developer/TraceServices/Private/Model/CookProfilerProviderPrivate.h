// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/ProviderLock.h"
#include "Containers/Map.h"
#include "TraceServices/Model/CookProfilerProvider.h"

namespace TraceServices
{

extern thread_local FProviderLock::FThreadLocalState GCookProviderLockState;

class FCookProfilerProvider
	: public ICookProfilerProvider
	, public IEditableCookProfilerProvider
{
public:
	explicit FCookProfilerProvider(IAnalysisSession& Session);
	virtual ~FCookProfilerProvider() {}

	//////////////////////////////////////////////////
	// Read operations

	virtual void BeginRead() const override       { Lock.BeginRead(GCookProviderLockState); }
	virtual void EndRead() const override         { Lock.EndRead(GCookProviderLockState); }
	virtual void ReadAccessCheck() const override { Lock.ReadAccessCheck(GCookProviderLockState); }

	virtual uint32 GetNumPackages() const;
	virtual void EnumeratePackages(double StartTime, double EndTime, EnumeratePackagesCallback Callback) const override;

	//////////////////////////////////////////////////
	// Edit operations

	virtual void BeginEdit() const override       { Lock.BeginWrite(GCookProviderLockState); }
	virtual void EndEdit() const override         { Lock.EndWrite(GCookProviderLockState); }
	virtual void EditAccessCheck() const override { Lock.WriteAccessCheck(GCookProviderLockState); }

	virtual FPackageData* EditPackage(uint64 Id) override;

	//////////////////////////////////////////////////

private:
	uint32 FindOrAddPackage(uint64 Id);

private:
	mutable FProviderLock Lock;

	IAnalysisSession& Session;

	TMap<uint64, uint32> PackageIdToIndexMap;
	TArray64<FPackageData> Packages;
};

} // namespace TraceServices
