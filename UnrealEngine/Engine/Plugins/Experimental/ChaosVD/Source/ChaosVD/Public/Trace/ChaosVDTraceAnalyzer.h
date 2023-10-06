// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Trace/Analyzer.h"
#include "Templates/SharedPointer.h"

struct FChaosVDStepData;
class FChaosVDTraceProvider;

namespace TraceServices { class IAnalysisSession; }

DECLARE_MULTICAST_DELEGATE(FChaosVDTraceAnalysisComplete)

/** Analyzer class for Chaos VD trace recordings.
 * It processes all Chaos VD trace events and rebuilds the recording in a usable
 * Recording struct handled by a custom Trace Provider
 */
class FChaosVDTraceAnalyzer final : public UE::Trace::IAnalyzer
{
public:
	FChaosVDTraceAnalyzer(TraceServices::IAnalysisSession& Session, const TSharedPtr<FChaosVDTraceProvider>& InChaosVDTraceProvider)
	: Session(Session),
	ChaosVDTraceProvider(InChaosVDTraceProvider)
	{
	}

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd() override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

	FChaosVDTraceAnalysisComplete& OnAnalysisComplete() { return ChaosVDTraceAnalysisCompleteDelegate; }

private:

	enum : uint16
	{
		RouteId_ChaosVDSolverStepStart,
		RouteId_ChaosVDSolverStepEnd,
		RouteId_ChaosVDSolverFrameStart,
		RouteId_ChaosVDSolverFrameEnd,
		RouteId_ChaosVDBinaryDataStart,
		RouteId_ChaosVDBinaryDataContent,
		RouteId_ChaosVDBinaryDataEnd,
		RouteId_ChaosVDSolverSimulationSpace,
		RouteId_BeginFrame,
		RouteId_EndFrame,
		RouteId_ChaosVDParticleDestroyed,
	};

	TraceServices::IAnalysisSession& Session;

	TSharedPtr<FChaosVDTraceProvider> ChaosVDTraceProvider;
	
	FChaosVDTraceAnalysisComplete ChaosVDTraceAnalysisCompleteDelegate;
};
