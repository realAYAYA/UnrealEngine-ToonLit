// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

namespace TraceServices { class IAnalysisSession; }
class FMenuBuilder;

namespace Insights
{

class ITimingViewSession;
extern TRACEINSIGHTS_API const FName TimingViewExtenderFeatureName;

class TRACEINSIGHTS_API ITimingViewExtender : public IModularFeature
{
public:
	virtual ~ITimingViewExtender() = default;

	/** Called to set up any data at the end of the timing view session */
	virtual void OnBeginSession(ITimingViewSession& InSession) = 0;

	/** Called to clear out any data at the end of the timing view session */
	virtual void OnEndSession(ITimingViewSession& InSession) = 0;

	/** Called each frame. If any new tracks are created they can be added via ITimingViewSession::Add*Track() */
	virtual void Tick(ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) = 0;

	/** Extension hook for the 'CPU Tracks Filter' menu */
	virtual void ExtendCpuTracksFilterMenu(ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) {}
	/** Extension hook for the 'GPU Tracks Filter' menu */
	virtual void ExtendGpuTracksFilterMenu(ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) {}
	/** Extension hook for the 'Other Tracks Filter' menu */
	virtual void ExtendOtherTracksFilterMenu(ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) {}
	/** Extension hook for the 'Plugins' menu */
	virtual void ExtendFilterMenu(ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) {}

	/** Extension hook for the context menu for all tracks
	@return True if any menu option was added and False if no option was added */ 
	virtual bool ExtendGlobalContextMenu(ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) { return false; }

	/** Allows extender to add filters to the Quick Find widget. */
	virtual void AddQuickFindFilters(TSharedPtr<class FFilterConfigurator> FilterConfigurator) {}
};

} // namespace Insights
