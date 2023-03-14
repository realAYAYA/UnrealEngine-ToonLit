// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Insights/ViewModels/StatsAggregator.h"
#include "Insights/ViewModels/StatsNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FCounterAggregator : public FStatsAggregator
{
public:
	FCounterAggregator() : FStatsAggregator(TEXT("Counters")) {}
	virtual ~FCounterAggregator() {}

	void ApplyResultsTo(const TMap<uint32, FStatsNodePtr>& StatsNodesIdMap) const;
	void ResetResults();

protected:
	virtual IStatsAggregationWorker* CreateWorker(TSharedPtr<const TraceServices::IAnalysisSession> InSession) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
