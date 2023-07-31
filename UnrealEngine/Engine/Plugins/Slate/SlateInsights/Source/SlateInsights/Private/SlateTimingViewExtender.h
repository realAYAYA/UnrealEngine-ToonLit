// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Insights/ITimingViewExtender.h"
#include "SlateTimingViewSession.h"

namespace Insights { class ITimingViewSession; }
namespace TraceServices { class IAnalysisSession; }

class FMenuBuilder;

namespace UE
{
namespace SlateInsights
{

class FSlateTimingViewExtender : public Insights::ITimingViewExtender
{
public:
	//~ Begin Insights::ITimingViewExtender interface
	virtual void OnBeginSession(Insights::ITimingViewSession& InSession) override;
	virtual void OnEndSession(Insights::ITimingViewSession& InSession) override;
	virtual void Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	virtual void ExtendFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;
	//~ End Insights::ITimingViewExtender interface

private:
	struct FPerSessionData
	{
		TUniquePtr<FSlateTimingViewSession> SharedData;
	};

	// The data we host per-session
	TMap<Insights::ITimingViewSession*, FPerSessionData> PerSessionDataMap;
};

} //namespace SlateInsights
} //namespace UE
