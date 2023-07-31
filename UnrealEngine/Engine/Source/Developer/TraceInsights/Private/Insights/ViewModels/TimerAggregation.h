// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Insights/ViewModels/StatsAggregator.h"
#include "TraceServices/Containers/Tables.h"
#include "TraceServices/Model/TimingProfiler.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimerAggregator : public FStatsAggregator
{
public:
	FTimerAggregator() : FStatsAggregator(TEXT("Timers")) {}
	virtual ~FTimerAggregator() {}

	TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* GetResultTable() const;
	void ResetResults();

protected:
	virtual IStatsAggregationWorker* CreateWorker(TSharedPtr<const TraceServices::IAnalysisSession> InSession) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
