// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "Model/TimingProfilerPrivate.h"

namespace TraceServices
{

	class FAnalysisSession;

class FGpuProfilerAnalyzer
	: public UE::Trace::IAnalyzer
{
public:
	FGpuProfilerAnalyzer(FAnalysisSession& Session, FTimingProfilerProvider& TimingProfilerProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_EventSpec,
		RouteId_Frame, // GPU Index 0
		RouteId_Frame2, // GPU Index 1
	};

	double GpuTimestampToSessionTime(uint64 GpuMicroseconds);

	FAnalysisSession& Session;
	FTimingProfilerProvider& TimingProfilerProvider;
	FTimingProfilerProvider::TimelineInternal& Timeline;
	FTimingProfilerProvider::TimelineInternal& Timeline2;
	TMap<uint64, uint32> EventTypeMap;
	double MinTime = 0.0f;
	double MinTime2 = 0.0f;
};

} // namespace TraceServices
