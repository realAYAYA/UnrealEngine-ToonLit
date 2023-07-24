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
	{
		if (!bIsDefault)
		{
			LoadFromConfig();
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
	}

	void SaveToConfig()
	{
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("DefaultZoomLevel"), DefaultZoomLevel, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoHideEmptyTracks"), bAutoHideEmptyTracks, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAllowPanningOnScreenEdges"), bAllowPanningOnScreenEdges, SettingsIni);
		GConfig->SetBool(TEXT("Insights.TimingProfiler"), TEXT("bAutoZoomOnFrameSelection"), bAutoZoomOnFrameSelection, SettingsIni);

		// Auto-scroll options
		const TCHAR* FrameAlignment = (AutoScrollFrameAlignment == 0) ? TEXT("game") : (AutoScrollFrameAlignment == 1) ? TEXT("rendering") : TEXT("none");
		GConfig->SetString(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollFrameAlignment"), FrameAlignment, SettingsIni);
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollViewportOffsetPercent"), AutoScrollViewportOffsetPercent, SettingsIni);
		GConfig->SetDouble(TEXT("Insights.TimingProfiler"), TEXT("AutoScrollMinDelay"), AutoScrollMinDelay, SettingsIni);

		GConfig->SetArray(TEXT("Insights.MemoryProfiler"), TEXT("SymbolSearchPaths"), SymbolSearchPaths, SettingsIni);

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

	/**
	 * Viewport offset while auto-scrolling, as percent of viewport width.
	 * If positive, it offsets the viewport forward, allowing an empty space at the right side of the viewport (i.e. after end of session).
	 * If negative, it offsets the viewport backward (i.e. end of session will be outside viewport).
	 */
	double AutoScrollViewportOffsetPercent;

	/** Minimum time between two auto-scroll updates, in [seconds]. */
	double AutoScrollMinDelay;
};
