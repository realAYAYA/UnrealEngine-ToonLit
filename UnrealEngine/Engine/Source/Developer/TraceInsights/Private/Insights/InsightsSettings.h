// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "ProfilingDebugging/MiscTrace.h"

/** Contains all settings for the Unreal Insights, accessible through the main manager. */
class FInsightsSettings
{
	friend class SInsightsSettings;

public:
	FInsightsSettings(bool bInIsDefault = false)
		: bIsEditing(false)
		, bIsDefault(bInIsDefault)
		, DefaultZoomLevel(5.0) // 5 seconds between major tick marks
		, bAutoHideEmptyTracks(true)
		, bAllowPanningOnScreenEdges(false)
		, bAutoZoomOnFrameSelection(false)
		, AutoScrollFrameAlignment((int32)TraceFrameType_Game) // -1 = none, 0 = game, 1 = rendering
		, AutoScrollViewportOffsetPercent(0.1) // scrolls forward 10% of viewport's width
		, AutoScrollMinDelay(0.3) // [seconds]
		, TimersViewMode((int32)TraceFrameType_Count)
		, TimersViewGroupingMode(3) // ByType
		, bTimersViewShowCpuTimers(true)
		, bTimersViewShowGpuTimers(true)
		, bTimersViewShowZeroCountTimers(true)
		, bTimingViewMainGraphShowPoints(false)
		, bTimingViewMainGraphShowPointsWithBorder(true)
		, bTimingViewMainGraphShowConnectedLines(true)
		, bTimingViewMainGraphShowPolygons(true)
		, bTimingViewMainGraphShowEventDuration(true)
		, bTimingViewMainGraphShowBars(false)
		, bTimingViewMainGraphShowGameFrames(true)
		, bTimingViewMainGraphShowRenderingFrames(true)
	{
		if (!bIsDefault)
		{
			LoadFromConfig();
		}
		else
		{
			TimersViewInstanceVisibleColumns.Add(TEXT("Count"));
			TimersViewInstanceVisibleColumns.Add(TEXT("TotalInclTime"));
			TimersViewInstanceVisibleColumns.Add(TEXT("TotalExclTime"));

			TimersViewGameFrameVisibleColumns.Add(TEXT("MaxInclTime"));
			TimersViewGameFrameVisibleColumns.Add(TEXT("AverageInclTime"));
			TimersViewGameFrameVisibleColumns.Add(TEXT("MedianInclTime"));
			TimersViewGameFrameVisibleColumns.Add(TEXT("MinInclTime"));

			TimersViewRenderingFrameVisibleColumns.Add(TEXT("MaxInclTime"));
			TimersViewRenderingFrameVisibleColumns.Add(TEXT("AverageInclTime"));
			TimersViewRenderingFrameVisibleColumns.Add(TEXT("MedianInclTime"));
			TimersViewRenderingFrameVisibleColumns.Add(TEXT("MinInclTime"));
		}
	}

	~FInsightsSettings()
	{
	}

	void LoadFromConfig()
	{
		if (!FConfigContext::ReadIntoGConfig().Load(TEXT("UnrealInsightsSettings"), SettingsIni))
		{
			return;
		}

		GConfig->GetDouble(TEXT("Insights.TimingProfiler"), TEXT("DefaultZoomLevel"), DefaultZoomLevel, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoHideEmptyTracks"), bAutoHideEmptyTracks, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler"), TEXT("bAllowPanningOnScreenEdges"), bAllowPanningOnScreenEdges, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoZoomOnFrameSelection"), bAutoZoomOnFrameSelection, SettingsIni);

		// Auto-scroll options
		GConfig->GetBool(TEXT("Insights.AutoScroll"), TEXT("bAutoScroll"), bAutoScroll, SettingsIni);
		FString FrameAlignment;
		if (GConfig->GetString(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollFrameAlignment"), FrameAlignment, SettingsIni))
		{
			FrameAlignment.TrimStartAndEndInline();
			if (FrameAlignment.Equals(TEXT("game"), ESearchCase::IgnoreCase))
			{
				static_assert((int32)TraceFrameType_Game == 0, "ETraceFrameType");
				AutoScrollFrameAlignment = (int32)TraceFrameType_Game;
			}
			else if (FrameAlignment.Equals(TEXT("rendering"), ESearchCase::IgnoreCase))
			{
				static_assert((int32)TraceFrameType_Rendering == 1, "ETraceFrameType");
				AutoScrollFrameAlignment = (int32)TraceFrameType_Rendering;
			}
			else
			{
				AutoScrollFrameAlignment = -1;
			}
		}
		GConfig->GetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollViewportOffsetPercent"), AutoScrollViewportOffsetPercent, SettingsIni);
		GConfig->GetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollMinDelay"), AutoScrollMinDelay, SettingsIni);

		GConfig->GetArray(TEXT("Insights.MemoryProfiler"), TEXT("SymbolSearchPaths"), SymbolSearchPaths, SettingsIni);
		
		GConfig->GetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("InstanceColumns"), TimersViewInstanceVisibleColumns, SettingsIni);
		GConfig->GetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("GameFrameColumns"), TimersViewGameFrameVisibleColumns, SettingsIni);
		GConfig->GetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("RenderingFrameColumns"), TimersViewRenderingFrameVisibleColumns, SettingsIni);

		GConfig->GetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("Mode"), TimersViewMode, SettingsIni);

		GConfig->GetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("GroupingMode"), TimersViewGroupingMode, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowCpuTimers"), bTimersViewShowCpuTimers, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowGpuTimers"), bTimersViewShowGpuTimers, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowZeroCountTimers"), bTimersViewShowZeroCountTimers, SettingsIni);

		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPoints"), bTimingViewMainGraphShowPoints, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPointsWithBorder"), bTimingViewMainGraphShowPointsWithBorder, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowConnectedLines"), bTimingViewMainGraphShowConnectedLines, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPolygons"), bTimingViewMainGraphShowPolygons, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowEventDuration"), bTimingViewMainGraphShowEventDuration, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowBars"), bTimingViewMainGraphShowBars, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowGameFrames"), bTimingViewMainGraphShowGameFrames, SettingsIni);
		GConfig->GetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowRenderingFrame"), bTimingViewMainGraphShowRenderingFrames, SettingsIni);

	}

	void SaveToConfig()
	{
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("DefaultZoomLevel"), DefaultZoomLevel, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoHideEmptyTracks"), bAutoHideEmptyTracks, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAllowPanningOnScreenEdges"), bAllowPanningOnScreenEdges, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoZoomOnFrameSelection"), bAutoZoomOnFrameSelection, SettingsIni);

		// Auto-scroll options
		GConfig->SetBool(TEXT("Insights.AutoScroll"), TEXT("bAutoScroll"), bAutoScroll, SettingsIni);
		const TCHAR* FrameAlignment = (AutoScrollFrameAlignment == 0) ? TEXT("game") : (AutoScrollFrameAlignment == 1) ? TEXT("rendering") : TEXT("none");
		GConfig->SetString(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollFrameAlignment"), FrameAlignment, SettingsIni);
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollViewportOffsetPercent"), AutoScrollViewportOffsetPercent, SettingsIni);
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollMinDelay"), AutoScrollMinDelay, SettingsIni);

		GConfig->SetArray(TEXT("Insights.MemoryProfiler"), TEXT("SymbolSearchPaths"), SymbolSearchPaths, SettingsIni);

		GConfig->SetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("InstanceColumns"), TimersViewInstanceVisibleColumns, SettingsIni);
		GConfig->SetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("GameFrameColumns"), TimersViewGameFrameVisibleColumns, SettingsIni);
		GConfig->SetArray(TEXT("Insights.TimingProfiler.TimersView"), TEXT("RenderingFrameColumns"), TimersViewRenderingFrameVisibleColumns, SettingsIni);

		GConfig->SetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("Mode"), TimersViewMode, SettingsIni);

		GConfig->SetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("GroupingMode"), TimersViewGroupingMode, SettingsIni);
		GConfig->SetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowCpuTimers"), bTimersViewShowCpuTimers, SettingsIni);
		GConfig->SetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowGpuTimers"), bTimersViewShowGpuTimers, SettingsIni);
		GConfig->SetInt(TEXT("Insights.TimingProfiler.TimersView"), TEXT("ShowZeroCountTimers"), bTimersViewShowZeroCountTimers, SettingsIni);

		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPoints"), bTimingViewMainGraphShowPoints, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPointsWithBorder"), bTimingViewMainGraphShowPointsWithBorder, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowConnectedLines"), bTimingViewMainGraphShowConnectedLines, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowPolygons"), bTimingViewMainGraphShowPolygons, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowEventDuration"), bTimingViewMainGraphShowEventDuration, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowBars"), bTimingViewMainGraphShowBars, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowGameFrames"), bTimingViewMainGraphShowGameFrames, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler.MainGraph"), TEXT("ShowRenderingFrame"), bTimingViewMainGraphShowRenderingFrames, SettingsIni);

		GConfig->Flush(false, SettingsIni);
	}

	void EnterEditMode()
	{
		bIsEditing = true;
	}

	void ExitEditMode()
	{
		bIsEditing = false;
	}

	const bool IsEditing() const
	{
		return bIsEditing;
	}

	const FInsightsSettings& GetDefaults() const
	{
		return Defaults;
	}

	void ResetToDefaults()
	{
		DefaultZoomLevel = Defaults.DefaultZoomLevel;
		bAutoHideEmptyTracks = Defaults.bAutoHideEmptyTracks;
		bAllowPanningOnScreenEdges = Defaults.bAllowPanningOnScreenEdges;
		bAutoZoomOnFrameSelection = Defaults.bAutoZoomOnFrameSelection;
		AutoScrollFrameAlignment = Defaults.AutoScrollFrameAlignment;
		AutoScrollViewportOffsetPercent = Defaults.AutoScrollViewportOffsetPercent;
		AutoScrollMinDelay = Defaults.AutoScrollMinDelay;
	}

	#define SET_AND_SAVE(Option, Value) { if (Option != Value) { Option = Value; SaveToConfig(); } }

	double GetDefaultZoomLevel() const { return DefaultZoomLevel; }
	void SetDefaultZoomLevel(double ZoomLevel) { DefaultZoomLevel = ZoomLevel; }
	void SetAndSaveDefaultZoomLevel(double ZoomLevel) { SET_AND_SAVE(DefaultZoomLevel, ZoomLevel); }

	bool IsAutoHideEmptyTracksEnabled() const { return bAutoHideEmptyTracks; }
	void SetAutoHideEmptyTracks(bool bOnOff) { bAutoHideEmptyTracks = bOnOff; }
	void SetAndSaveAutoHideEmptyTracks(bool bOnOff) { SET_AND_SAVE(bAutoHideEmptyTracks, bOnOff); }

	bool IsPanningOnScreenEdgesEnabled() const { return bAllowPanningOnScreenEdges; }
	void SetPanningOnScreenEdges(bool bOnOff) { bAllowPanningOnScreenEdges = bOnOff; }
	void SetAndSavePanningOnScreenEdges(bool bOnOff) { SET_AND_SAVE(bAllowPanningOnScreenEdges, bOnOff); }

	bool IsAutoZoomOnFrameSelectionEnabled() const { return bAutoZoomOnFrameSelection; }
	void SetAutoZoomOnFrameSelection(bool bOnOff) { bAutoZoomOnFrameSelection = bOnOff; }
	void SetAndSaveAutoZoomOnFrameSelection(bool bOnOff) { SET_AND_SAVE(bAutoZoomOnFrameSelection, bOnOff); }

	bool IsAutoScrollEnabled() const { return bAutoScroll; }
	void SetAutoScroll(bool bOnOff) { bAutoScroll = bOnOff; }
	void SetAndSaveAutoScroll(bool bOnOff) { SET_AND_SAVE(bAutoScroll, bOnOff); }

	int32 GetAutoScrollFrameAlignment() const { return AutoScrollFrameAlignment; }
	void SetAutoScrollFrameAlignment(int32 FrameType) { AutoScrollFrameAlignment = FrameType; }
	void SetAndSaveAutoScrollFrameAlignment(int32 FrameType) { SET_AND_SAVE(AutoScrollFrameAlignment, FrameType); }

	double GetAutoScrollViewportOffsetPercent() const { return AutoScrollViewportOffsetPercent; }
	void SetAutoScrollViewportOffsetPercent(double OffsetPercent) { AutoScrollViewportOffsetPercent = OffsetPercent; }
	void SetAndSaveAutoScrollViewportOffsetPercent(double OffsetPercent) { SET_AND_SAVE(AutoScrollViewportOffsetPercent, OffsetPercent); }

	double GetAutoScrollMinDelay() const { return AutoScrollMinDelay; }
	void SetAutoScrollMinDelay(double Delay) { AutoScrollMinDelay = Delay; }
	void SetAndSaveAutoScrollMinDelay(double Delay) { SET_AND_SAVE(AutoScrollMinDelay, Delay); }

	const TArray<FString>& GetSymbolSearchPaths() const { return SymbolSearchPaths; }
	void SetSymbolSearchPaths(const TArray<FString>& SearchPaths) { SymbolSearchPaths = SearchPaths; }
	void SetAndSaveSymbolSearchPaths(const TArray<FString>& SearchPaths) { SET_AND_SAVE(SymbolSearchPaths, SearchPaths); }

	const TArray<FString>& GetTimersViewInstanceVisibleColumns() const { return TimersViewInstanceVisibleColumns; }
	void SetTimersViewInstanceVisibleColumns(const TArray<FString>& Columns) { TimersViewInstanceVisibleColumns = Columns; }
	void SetAndSaveTimersViewInstanceVisibleColumns(const TArray<FString>& Columns) { SET_AND_SAVE(TimersViewInstanceVisibleColumns, Columns); }

	const TArray<FString>& GetTimersViewGameFrameVisibleColumns() const { return TimersViewGameFrameVisibleColumns; }
	void SetTimersViewGameFrameVisibleColumns(const TArray<FString>& Columns) { TimersViewGameFrameVisibleColumns = Columns; }
	void SetAndSaveTimersViewGameFrameVisibleColumns(const TArray<FString>& Columns) { SET_AND_SAVE(TimersViewGameFrameVisibleColumns, Columns); }

	const TArray<FString>& GetTimersViewRenderingFrameVisibleColumns() const { return TimersViewRenderingFrameVisibleColumns; }
	void SetTimersViewRenderingFrameVisibleColumns(const TArray<FString>& Columns) { TimersViewRenderingFrameVisibleColumns = Columns; }
	void SetAndSaveTimersViewRenderingFrameVisibleColumns(const TArray<FString>& Columns) { SET_AND_SAVE(TimersViewRenderingFrameVisibleColumns, Columns); }

	int32 GetTimersViewMode() const { return TimersViewMode; }
	void SetTimersViewMode(int32 InMode) { TimersViewMode = InMode; }
	void SetAndSaveTimersViewMode(int32 InMode) { SET_AND_SAVE(TimersViewMode, InMode); }

	int32 GetTimersViewGroupingMode() const { return TimersViewGroupingMode; }
	void SetTimersViewGroupingMode(int32 InValue) { TimersViewGroupingMode = InValue; }
	void SetAndSaveTimersViewGroupingMode(int32 InValue) { SET_AND_SAVE(TimersViewGroupingMode, InValue); }

	bool GetTimersViewShowCpuEvents() const { return bTimersViewShowCpuTimers; }
	void SetTimersViewShowCpuEvents(bool InValue) { bTimersViewShowCpuTimers = InValue; }
	void SetAndSaveTimersViewShowCpuEvents(bool InValue) { SET_AND_SAVE(bTimersViewShowCpuTimers, InValue); }

	bool GetTimersViewShowGpuEvents() const { return bTimersViewShowGpuTimers; }
	void SetTimersViewShowGpuEvents(bool InValue) { bTimersViewShowGpuTimers = InValue; }
	void SetAndSaveTimersViewShowGpuEvents(bool InValue) { SET_AND_SAVE(bTimersViewShowGpuTimers, InValue); }

	bool GetTimersViewShowZeroCountTimers() const { return bTimersViewShowZeroCountTimers; }
	void SetTimersViewShowZeroCountTimers(bool InValue) { bTimersViewShowZeroCountTimers = InValue; }
	void SetAndSaveTimersViewShowZeroCountTimers(bool InValue) { SET_AND_SAVE(bTimersViewShowZeroCountTimers, InValue); }

	bool GetTimingViewMainGraphShowPoints() const { return bTimingViewMainGraphShowPoints; }
	void SetTimingViewMainGraphShowPoints(bool InValue) { bTimingViewMainGraphShowPoints = InValue; }
	void SetAndSaveTimingViewMainGraphShowPoints(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowPoints, InValue); }

	bool GetTimingViewMainGraphShowPointsWithBorder() const { return bTimingViewMainGraphShowPointsWithBorder; }
	void SetTimingViewMainGraphShowPointsWithBorder(bool InValue) { bTimingViewMainGraphShowPointsWithBorder = InValue; }
	void SetAndSaveTimingViewMainGraphShowPointsWithBorder(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowPointsWithBorder, InValue); }

	bool GetTimingViewMainGraphShowConnectedLines() const { return bTimingViewMainGraphShowConnectedLines; }
	void SetTimingViewMainGraphShowConnectedLines(bool InValue) { bTimingViewMainGraphShowConnectedLines = InValue; }
	void SetAndSaveTimingViewMainGraphShowConnectedLines(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowConnectedLines, InValue); }

	bool GetTimingViewMainGraphShowPolygons() const { return bTimingViewMainGraphShowPolygons; }
	void SetTimingViewMainGraphShowPolygons(bool InValue) { bTimingViewMainGraphShowPolygons = InValue; }
	void SetAndTimingViewMainGraphShowPolygons(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowPolygons, InValue); }

	bool GetTimingViewMainGraphShowEventDuration() const { return bTimingViewMainGraphShowEventDuration; }
	void SetTimingViewMainGraphShowEventDuration(bool InValue) { bTimingViewMainGraphShowEventDuration = InValue; }
	void SetAndSaveTimingViewMainGraphShowEventDuration(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowEventDuration, InValue); }

	bool GetTimingViewMainGraphShowBars() const { return bTimingViewMainGraphShowBars; }
	void SetTimingViewMainGraphShowBars(bool InValue) { bTimingViewMainGraphShowBars = InValue; }
	void SetAndSaveTimingViewMainGraphShowBars(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowBars, InValue); }

	bool GetTimingViewMainGraphShowGameFrames() const { return bTimingViewMainGraphShowGameFrames; }
	void SetTimingViewMainGraphShowGameFrames(bool InValue) { bTimingViewMainGraphShowGameFrames = InValue; }
	void SetAndSaveTimingViewMainGraphShowGameFrames(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowGameFrames, InValue); }

	bool GetTimingViewMainGraphShowRenderingFrames() const { return bTimingViewMainGraphShowRenderingFrames; }
	void SetTimingViewMainGraphShowRenderingFrames(bool InValue) { bTimingViewMainGraphShowRenderingFrames = InValue; }
	void SetAndSaveTimingViewMainGraphShowRenderingFrames(bool InValue) { SET_AND_SAVE(bTimingViewMainGraphShowRenderingFrames, InValue); }

	#undef SET_AND_SAVE

private:
	/** Contains default settings. */
	static FInsightsSettings Defaults;

	/** Setting filename ini. */
	FString SettingsIni;

	/** Whether profiler settings is in edit mode. */
	bool bIsEditing;

	/** Whether this instance contains defaults. */
	bool bIsDefault;

	//////////////////////////////////////////////////
	// Actual settings.

	/** The default (initial) zoom level of the Timing view. */
	double DefaultZoomLevel;

	/** Auto hide empty tracks (ex.: ones without timing events in the current viewport). */
	bool bAutoHideEmptyTracks;

	/** If enabled, the panning is allowed to continue when mouse cursor reaches the edges of the screen. */
	bool bAllowPanningOnScreenEdges;

	/** If enabled, the Timing View will also be zoomed when a new frame is selected in the Frames track. */
	bool bAutoZoomOnFrameSelection;

	/** -1 to disable frame alignment or the type of frame to align with (0 = Game or 1 = Rendering). */
	int32 AutoScrollFrameAlignment;

	/** List of search paths to look for symbol files */
	TArray<FString> SymbolSearchPaths;

	/** If enabled, the Timing View will start with auto-scroll enabled. */
	bool bAutoScroll;

	/**
	 * Viewport offset while auto-scrolling, as percent of viewport width.
	 * If positive, it offsets the viewport forward, allowing an empty space at the right side of the viewport (i.e. after end of session).
	 * If negative, it offsets the viewport backward (i.e. end of session will be outside viewport).
	 */
	double AutoScrollViewportOffsetPercent;

	/** Minimum time between two auto-scroll updates, in [seconds]. */
	double AutoScrollMinDelay;

	//** The list of visible columns in the Timers view in the Instance mode. */
	TArray<FString> TimersViewInstanceVisibleColumns;

	//** The list of visible columns in the Timers view in the Game Frame mode. */
	TArray<FString> TimersViewGameFrameVisibleColumns;

	//** The list of visible columns in the Timers view in the Rendering Frame mode. */
	TArray<FString> TimersViewRenderingFrameVisibleColumns;

	//** The mode for the timers panel. */
	int32 TimersViewMode;

	//** The grouping mode for the timers panel. */
	int32 TimersViewGroupingMode;

	//** If enabled, Cpu timers will be displayed in the Timing View. */
	bool bTimersViewShowCpuTimers;

	//** If enabled, Gpu timers will be displayed in the Timing View. */
	bool bTimersViewShowGpuTimers;

	//** If enabled, timers with no instances in the selected interval will still be displayed in the Timers View. */
	bool bTimersViewShowZeroCountTimers;

	//** If enabled, values will be displayed as points in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowPoints;

	//** If enabled, values will be displayed as points with border in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowPointsWithBorder;

	//** If enabled, values will be displayed as connected lines in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowConnectedLines;

	//** If enabled, values will be displayed as polygons in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowPolygons;

	//** If enabled, uses duration of timing events for connected lines and polygons in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowEventDuration;

	//** If enabled, shows bars corresponding to the duration of the timing events in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowBars;

	//** If enabled, shows game frames in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowGameFrames;

	//** If enabled, shows rendering frames in the Main Graph Track in Timing Insights. */
	bool bTimingViewMainGraphShowRenderingFrames;
};
