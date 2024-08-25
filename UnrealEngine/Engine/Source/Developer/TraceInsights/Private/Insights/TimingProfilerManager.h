// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Framework/Commands/UICommandList.h"

// Insights
#include "Insights/InsightsManager.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/TimingProfilerCommands.h"
#include "Insights/ViewModels/TimerNode.h"

namespace Insights
{
	class FTimerButterflyAggregator;

	enum class ETimingEventsColoringMode : uint32
	{
		ByTimerName,
		ByTimerId,
		BySourceFile,
		ByDuration,

		Count
	};
}

class STimingProfilerWindow;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This class manages the Timing Profiler (Timing Insights) state and settings.
 */
class FTimingProfilerManager : public TSharedFromThis<FTimingProfilerManager>, public IInsightsComponent
{
	friend class FTimingProfilerActionManager;

public:
	inline static constexpr uint32 UnlimitedEventDepth = 1000;

public:
	/** Creates the Timing Profiler manager, only one instance can exist. */
	FTimingProfilerManager(TSharedRef<FUICommandList> InCommandList);

	/** Virtual destructor. */
	virtual ~FTimingProfilerManager();

	/** Creates an instance of the Timing Profiler manager. */
	static TSharedPtr<FTimingProfilerManager> CreateInstance();

	/**
	 * @return the global instance of the Timing Profiler manager.
	 * This is an internal singleton and cannot be used outside TraceInsights.
	 * For external use:
	 *     IUnrealInsightsModule& Module = FModuleManager::Get().LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");
	 *     Module.GetTimingProfiler();
	 */
	static TSharedPtr<FTimingProfilerManager> Get();

	//////////////////////////////////////////////////
	// IInsightsComponent

	virtual void Initialize(IUnrealInsightsModule& InsightsModule) override;
	virtual void Shutdown() override;
	virtual void RegisterMajorTabs(IUnrealInsightsModule& InsightsModule) override;
	virtual void UnregisterMajorTabs() override;
	virtual void OnWindowClosedEvent() override;
	virtual bool Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

	//////////////////////////////////////////////////

	/** @return UI command list for the Timing Profiler manager. */
	const TSharedRef<FUICommandList> GetCommandList() const;

	/** @return an instance of the Timing Profiler commands. */
	static const FTimingProfilerCommands& GetCommands();

	/** @return an instance of the Timing Profiler action manager. */
	static FTimingProfilerActionManager& GetActionManager();

	/**
	 * Converts profiler window weak pointer to a shared pointer and returns it.
	 * Make sure the returned pointer is valid before trying to dereference it.
	 */
	TSharedPtr<STimingProfilerWindow> GetProfilerWindow() const
	{
		return ProfilerWindowWeakPtr.Pin();
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Getters and setters used by Toggle Commands.

	/** @return true, if the Frames track/view is visible */
	const bool IsFramesTrackVisible() const { return bIsFramesTrackVisible; }
	void SetFramesTrackVisible(const bool bIsVisible) { bIsFramesTrackVisible = bIsVisible; }
	void ShowHideFramesTrack(const bool bIsVisible);

	/** @return true, if the Timing view is visible */
	const bool IsTimingViewVisible() const { return bIsTimingViewVisible; }
	void SetTimingViewVisible(const bool bIsVisible) { bIsTimingViewVisible = bIsVisible; }
	void ShowHideTimingView(const bool bIsVisible);

	/** @return true, if the Timers view is visible */
	const bool IsTimersViewVisible() const { return bIsTimersViewVisible; }
	void SetTimersViewVisible(const bool bIsVisible) { bIsTimersViewVisible = bIsVisible; }
	void ShowHideTimersView(const bool bIsVisible);

	/** @return true, if the Callers tree view is visible */
	const bool IsCallersTreeViewVisible() const { return bIsCallersTreeViewVisible; }
	void SetCallersTreeViewVisible(const bool bIsVisible) { bIsCallersTreeViewVisible = bIsVisible; }
	void ShowHideCallersTreeView(const bool bIsVisible);

	/** @return true, if the Callees tree view is visible */
	const bool IsCalleesTreeViewVisible() const { return bIsCalleesTreeViewVisible; }
	void SetCalleesTreeViewVisible(const bool bIsVisible) { bIsCalleesTreeViewVisible = bIsVisible; }
	void ShowHideCalleesTreeView(const bool bIsVisible);

	/** @return true, if the Counters view is visible */
	const bool IsStatsCountersViewVisible() const { return bIsStatsCountersViewVisible; }
	void SetStatsCountersViewVisible(const bool bIsVisible) { bIsStatsCountersViewVisible = bIsVisible; }
	void ShowHideStatsCountersView(const bool bIsVisible);

	/** @return true, if the Log view is visible */
	const bool IsLogViewVisible() const { return bIsLogViewVisible; }
	void SetLogViewVisible(const bool bIsVisible) { bIsLogViewVisible = bIsVisible; }
	void ShowHideLogView(const bool bIsVisible);

	////////////////////////////////////////////////////////////////////////////////////////////////////

	void OnSessionChanged();

	double GetSelectionStartTime() const { return SelectionStartTime; }
	double GetSelectionEndTime() const { return SelectionEndTime; }
	void SetSelectedTimeRange(double StartTime, double EndTime);

	FTimerNodePtr GetTimerNode(uint32 TimerId) const;
	uint32 GetSelectedTimer() const { return SelectedTimerId; }
	void SetSelectedTimer(uint32 TimerId);
	void ToggleTimingViewMainGraphEventSeries(uint32 InTimerId);

	void OnThreadFilterChanged();

	void ResetCallersAndCallees();
	void UpdateCallersAndCallees();
	TSharedRef<Insights::FTimerButterflyAggregator> GetTimerButterflyAggregator() const { return TimerButterflyAggregator; }

	void UpdateAggregatedTimerStats();
	void UpdateAggregatedCounterStats();

	const FName& GetLogListingName() const { return LogListingName; }

	Insights::ETimingEventsColoringMode GetColoringMode() const { return ColoringMode; }
	void SetColoringMode(Insights::ETimingEventsColoringMode InColoringMode) { ColoringMode = InColoringMode; }

	uint32 GetEventDepthLimit() const { return EventDepthLimit; }
	void SetEventDepthLimit(uint32 InEventDepthLimit) { EventDepthLimit = InEventDepthLimit; }

private:
	/** Binds our UI commands to delegates. */
	void BindCommands();

	/** Called to spawn the Timing Profiler major tab. */
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	bool CanSpawnTab(const FSpawnTabArgs& Args) const;

	/** Callback called when the Timing Profiler major tab is closed. */
	void OnTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	/** Updates this manager, done through FCoreTicker. */
	bool Tick(float DeltaTime);

	void FinishTimerButterflyAggregation();

	void AssignProfilerWindow(const TSharedRef<STimingProfilerWindow>& InProfilerWindow)
	{
		ProfilerWindowWeakPtr = InProfilerWindow;
	}

	void RemoveProfilerWindow()
	{
		ProfilerWindowWeakPtr.Reset();
	}

private:
	bool bIsInitialized;
	bool bIsAvailable;
	FAvailabilityCheck AvailabilityCheck;

	/** The delegate to be invoked when this manager ticks. */
	FTickerDelegate OnTick;

	/** Handle to the registered OnTick. */
	FTSTicker::FDelegateHandle OnTickHandle;

	/** List of UI commands for this manager. This will be filled by this and corresponding classes. */
	TSharedRef<FUICommandList> CommandList;

	/** An instance of the Timing Profiler action manager. */
	FTimingProfilerActionManager ActionManager;

	/** A weak pointer to the Timing Insights window. */
	TWeakPtr<STimingProfilerWindow> ProfilerWindowWeakPtr;

	/** If the Frames Track is visible or hidden. */
	bool bIsFramesTrackVisible;

	/** If the Timing View is visible or hidden. */
	bool bIsTimingViewVisible;

	/** If the Timers View is visible or hidden. */
	bool bIsTimersViewVisible;

	/** If the Callers Tree View is visible or hidden. */
	bool bIsCallersTreeViewVisible;

	/** If the Callees Tree View is visible or hidden. */
	bool bIsCalleesTreeViewVisible;

	/** If the Stats Counters View is visible or hidden. */
	bool bIsStatsCountersViewVisible;

	/** If the Log View is visible or hidden. */
	bool bIsLogViewVisible;

	//////////////////////////////////////////////////

	double SelectionStartTime;
	double SelectionEndTime;

	static constexpr uint32 InvalidTimerId = uint32(-1);
	uint32 SelectedTimerId;

	TSharedRef<Insights::FTimerButterflyAggregator> TimerButterflyAggregator;

	/** The name of the Timing Insights log listing. */
	FName LogListingName;

	Insights::ETimingEventsColoringMode ColoringMode = Insights::ETimingEventsColoringMode::ByTimerName;
	uint32 EventDepthLimit = UnlimitedEventDepth;

	/** A shared pointer to the global instance of the Timing Profiler manager. */
	static TSharedPtr<FTimingProfilerManager> Instance;
};
