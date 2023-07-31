// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/ProviderLock.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{

struct FPackageData
{
	FPackageData(uint64 InId);
	uint64 Id = -1;
	const TCHAR* Name;
	double LoadTime = 0;
	double SaveTime = 0;
	double BeginCacheForCookedPlatformData = 0; // BeginCacheForCookedPlatformData is the name of the function from the cooker.
	double IsCachedCookedPlatformDataLoaded = 0; // IsCachedCookedPlatformDataLoaded is the name of the function from the cooker.
	const TCHAR* AssetClass;
};

class ICookProfilerProvider
	: public IProvider
{
public:
	typedef TFunctionRef<bool(const FPackageData& /*Package*/)> EnumeratePackagesCallback;

	virtual ~ICookProfilerProvider() = default;
	virtual void EnumeratePackages(double StartTime, double EndTime, EnumeratePackagesCallback Callback) const = 0;
	virtual uint32 GetNumPackages() const = 0;
};

TRACESERVICES_API const ICookProfilerProvider* ReadCookProfilerProvider(const IAnalysisSession& Session);
} // namespace TraceServices
