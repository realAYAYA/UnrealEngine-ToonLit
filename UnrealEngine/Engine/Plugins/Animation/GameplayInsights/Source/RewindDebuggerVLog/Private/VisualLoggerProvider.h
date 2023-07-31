// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IVisualLoggerProvider.h"
#include "Model/PointTimeline.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Model/IntervalTimeline.h"
#include "VisualLogger/VisualLoggerTypes.h"

namespace TraceServices { class IAnalysisSession; }

class FVisualLoggerProvider : public IVisualLoggerProvider
{
public:
	static FName ProviderName;

	FVisualLoggerProvider(TraceServices::IAnalysisSession& InSession);

	/** IVisualLoggerProvider interface */
	virtual bool ReadVisualLogEntryTimeline(uint64 InObjectId, TFunctionRef<void(const VisualLogEntryTimeline&)> Callback) const;

	/** Add an object event message */
	void AppendVisualLogEntry(uint64 InObjectId, double InTime, const FVisualLogEntry& Entry);

private:
	TraceServices::IAnalysisSession& Session;

	TMap<uint64, uint32> ObjectIdToLogEntryTimelines;
	TArray<TSharedRef<TraceServices::TPointTimeline<FVisualLogEntry>>> LogEntryTimelines;
};
