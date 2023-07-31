// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "AnalysisServicePrivate.h"

namespace TraceServices
{
class FCookProfilerProvider;

class FCookAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FCookAnalyzer(IAnalysisSession& Session, FCookProfilerProvider& CookProfilerProvider);
	virtual ~FCookAnalyzer();

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;
	
private:
	enum : uint16
	{
		RouteId_Package,
		RouteId_PackageStat,
		RouteId_PackageAssetClass,
	};

	IAnalysisSession& Session;
	FCookProfilerProvider& CookProfilerProvider;
};

} // namespace TraceServices
