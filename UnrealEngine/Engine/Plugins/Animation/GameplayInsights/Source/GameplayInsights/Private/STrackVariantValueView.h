// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FBaseTimingTrack;
namespace Insights { class ITimingViewSession; }
namespace TraceServices { class IAnalysisSession; }
namespace Insights { enum class ETimeChangedFlags; }
class SVariantValueView;

// Wrapper for a variant value view for a track
class STrackVariantValueView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STrackVariantValueView) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FBaseTimingTrack>& InTimingTrack, Insights::ITimingViewSession& InTimingViewSession, const TraceServices::IAnalysisSession& InAnalysisSession);

	TSharedPtr<FBaseTimingTrack> GetTimingTrack() const { return TimingTrack.Pin(); }

private:
	// Handle the time marker being scrubbed
	void HandleTimeMarkerChanged(Insights::ETimeChangedFlags InFlags, double InTimeMarker);

private:
	TWeakPtr<FBaseTimingTrack> TimingTrack;

	TSharedPtr<SVariantValueView> VariantValueView;

	const TraceServices::IAnalysisSession* AnalysisSession;
};
