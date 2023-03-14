// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Insights/ViewModels/TimingEventsTrack.h"
#include "TraceServices/Model/Frames.h"

namespace TraceServices { class IAnalysisSession; }
namespace Insights { class ITimingViewSession; }
namespace Insights { enum class ETimeChangedFlags : int32; }
class FMenuBuilder;
class SDockTab;

namespace UE
{
namespace RenderGraphInsights
{
class FRenderGraphTrack;

class FRenderGraphTimingViewSession
{
public:
	FRenderGraphTimingViewSession() = default;

	void OnBeginSession(Insights::ITimingViewSession& InTimingViewSession);
	void OnEndSession(Insights::ITimingViewSession& InTimingViewSession);
	void Tick(Insights::ITimingViewSession& InTimingViewSession, const TraceServices::IAnalysisSession& InAnalysisSession);
	void ExtendFilterMenu(FMenuBuilder& InMenuBuilder);

	/** Get the last cached analysis session */
	const TraceServices::IAnalysisSession& GetAnalysisSession() const { return *AnalysisSession; }

	/** Check whether the analysis session is valid */
	bool IsAnalysisSessionValid() const { return AnalysisSession != nullptr; }

	/** Show/Hide the RenderGraph track */
	void ToggleRenderGraphTrack();

	Insights::ITimingViewSession* GetTimingViewSession() const { return TimingViewSession; }

private:
	const TraceServices::IAnalysisSession* AnalysisSession = nullptr;
	Insights::ITimingViewSession* TimingViewSession;
	TSharedPtr<FRenderGraphTrack> Track;
	bool bTrackVisible = true;
};

} //namespace RenderGraphInsights
} //namespace UE
