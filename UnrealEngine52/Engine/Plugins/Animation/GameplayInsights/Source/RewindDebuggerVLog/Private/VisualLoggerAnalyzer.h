// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

class FVisualLoggerProvider;
namespace TraceServices { class IAnalysisSession; }

class FVisualLoggerAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FVisualLoggerAnalyzer(TraceServices::IAnalysisSession& InSession, FVisualLoggerProvider& InProvider);

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd() override {}
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_VisualLogEntry,
	};

	TraceServices::IAnalysisSession& Session;
	FVisualLoggerProvider& Provider;
};
