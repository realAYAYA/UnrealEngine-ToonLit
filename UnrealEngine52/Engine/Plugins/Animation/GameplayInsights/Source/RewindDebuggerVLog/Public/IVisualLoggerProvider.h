// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Containers/Timelines.h"
#include "VisualLogger/VisualLoggerTypes.h"

class IVisualLoggerProvider : public TraceServices::IProvider
{
public:
	typedef TraceServices::ITimeline<FVisualLogEntry> VisualLogEntryTimeline;

	virtual bool ReadVisualLogEntryTimeline(uint64 InObjectId, TFunctionRef<void(const VisualLogEntryTimeline&)> Callback) const = 0;
};

