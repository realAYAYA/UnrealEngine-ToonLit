// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Insights/ITimingViewExtender.h"

namespace Insights { class ITimingViewSession; }
class UWorld;
class FGameplaySharedData;
class FAnimationSharedData;
class IAnimationBlueprintEditor;
struct FCustomDebugObject;
class SGameplayInsightsTransportControls;

class FGameplayTimingViewExtender : public Insights::ITimingViewExtender
{
public:
	// Insights::ITimingViewExtender interface
	virtual void OnBeginSession(Insights::ITimingViewSession& InSession) override;
	virtual void OnEndSession(Insights::ITimingViewSession& InSession) override;
	virtual void Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	virtual void ExtendFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;

#if WITH_EDITOR
	// Gets a world to perform visualizations within, depending on context
	static UWorld* GetWorldToVisualize();

	// Get custom debug objects for integration with anim blueprint debugging
	void GetCustomDebugObjects(const IAnimationBlueprintEditor& InAnimationBlueprintEditor, TArray<FCustomDebugObject>& OutDebugList);
#endif

	// Tick the visualizers
	void TickVisualizers(float DeltaTime);

private:
	struct FPerSessionData
	{
		// Shared data
		FGameplaySharedData* GameplaySharedData;
		FAnimationSharedData* AnimationSharedData;
#if WITH_EDITOR
		TSharedPtr<SGameplayInsightsTransportControls> TransportControls;
#endif
	};

	// The data we host per-session
	TMap<Insights::ITimingViewSession*, FPerSessionData> PerSessionDataMap;
};
