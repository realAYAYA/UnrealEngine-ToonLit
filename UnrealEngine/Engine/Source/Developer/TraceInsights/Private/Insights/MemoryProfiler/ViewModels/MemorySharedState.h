// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "TraceServices/Model/AllocationsProvider.h"

// Insights
#include "Insights/ITimingViewExtender.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryGraphTrack.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTag.h"

class FTimingEventSearchParameters;
class FTimingGraphSeries;
class FTimingGraphTrack;
class STimingView;

namespace Insights
{

struct FReportConfig;
struct FReportTypeConfig;
struct FReportTypeGraphConfig;
class FMemoryTracker;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryRuleSpec
{
public:
	typedef TraceServices::IAllocationsProvider::EQueryRule ERule;

public:
	FMemoryRuleSpec(ERule InValue, uint32 InNumTimeMarkers, const FText& InShortName, const FText& InVerboseName, const FText& InDescription)
		: Value(InValue)
		, NumTimeMarkers(InNumTimeMarkers)
		, ShortName(InShortName)
		, VerboseName(InVerboseName)
		, Description(InDescription)
	{}

	ERule GetValue() const { return Value; }
	uint32 GetNumTimeMarkers() const { return NumTimeMarkers; }
	FText GetShortName() const { return ShortName; }
	FText GetVerboseName() const { return VerboseName; }
	FText GetDescription() const { return Description; }

private:
	ERule Value;           // ex.: ERule::AafB
	uint32 NumTimeMarkers; // ex.: 2
	FText ShortName;       // ex.: "A**B"
	FText VerboseName;     // ex.: "Short Living Allocations"
	FText Description;     // ex.: "Allocations allocated and freed between time A and time B (A <= a <= f <= B)."
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FQueryTargetWindowSpec
{
public:
	FQueryTargetWindowSpec(const FName& InName, const FText& InText)
		: Text(InText)
		, Name(InName)
	{}

	FText GetText() const { return Text; }
	FName GetName() const { return Name; }

public:
	static const FName NewWindow;

private:
	FText Text;
	FName Name;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryTimingViewCommands : public TCommands<FMemoryTimingViewCommands>
{
public:
	FMemoryTimingViewCommands();
	virtual ~FMemoryTimingViewCommands();
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> ShowHideAllMemoryTracks;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemorySharedState : public Insights::ITimingViewExtender, public TSharedFromThis<FMemorySharedState>
{
public:
	FMemorySharedState();
	virtual ~FMemorySharedState();

	TSharedPtr<STimingView> GetTimingView() const { return TimingView; }
	void SetTimingView(TSharedPtr<STimingView> InTimingView) { TimingView = InTimingView; }

	const Insights::FMemoryTagList& GetTagList() { return TagList; }

	const TArray<TSharedPtr<Insights::FMemoryTracker>>& GetTrackers()  const { return Trackers; }
	FString TrackersToString(uint64 Flags, const TCHAR* Conjunction) const;
	const Insights::FMemoryTracker* GetTrackerById(Insights::FMemoryTrackerId InMemTrackerId) const;

	TSharedPtr<FMemoryGraphTrack> GetMainGraphTrack() const { return MainGraphTrack; }

	EMemoryTrackHeightMode GetTrackHeightMode() const { return TrackHeightMode; }
	void SetTrackHeightMode(EMemoryTrackHeightMode InTrackHeightMode);

	//////////////////////////////////////////////////
	// ITimingViewExtender interface

	virtual void OnBeginSession(Insights::ITimingViewSession& InSession) override;
	virtual void OnEndSession(Insights::ITimingViewSession& InSession) override;
	virtual void Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	virtual void ExtendOtherTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder) override;

	//////////////////////////////////////////////////

	void BindCommands();

	bool IsAllMemoryTracksToggleOn() const { return bShowHideAllMemoryTracks; }
	void SetAllMemoryTracksToggle(bool bOnOff);
	void ShowAllMemoryTracks() { SetAllMemoryTracksToggle(true); }
	void HideAllMemoryTracks() { SetAllMemoryTracksToggle(false); }
	void ShowHideAllMemoryTracks() { SetAllMemoryTracksToggle(!IsAllMemoryTracksToggleOn()); }

	void CreateDefaultTracks();

	TSharedPtr<FMemoryGraphTrack> CreateMemoryGraphTrack();
	int32 RemoveMemoryGraphTrack(TSharedPtr<FMemoryGraphTrack> GraphTrack);

	TSharedPtr<FMemoryGraphTrack> GetMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	TSharedPtr<FMemoryGraphTrack> CreateMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	void RemoveTrackFromMemTags(TSharedPtr<FMemoryGraphTrack>& GraphTrack);
	int32 RemoveMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	int32 RemoveAllMemTagGraphTracks();

	TSharedPtr<FMemoryGraphSeries> ToggleMemTagGraphSeries(TSharedPtr<FMemoryGraphTrack> InGraphTrack, Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);

	void CreateTracksFromReport(const FString& Filename);
	void CreateTracksFromReport(const Insights::FReportConfig& ReportConfig);
	void CreateTracksFromReport(const Insights::FReportTypeConfig& ReportTypeConfig);

	const TArray<TSharedPtr<Insights::FMemoryRuleSpec>>& GetMemoryRules() const { return MemoryRules; }
	
	TSharedPtr<Insights::FMemoryRuleSpec> GetCurrentMemoryRule() const { return CurrentMemoryRule; }
	void SetCurrentMemoryRule(TSharedPtr<Insights::FMemoryRuleSpec> InRule) { CurrentMemoryRule = InRule; OnMemoryRuleChanged(); }

	const TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>>& GetQueryTargets() const { return QueryTargetSpecs; }

	TSharedPtr<Insights::FQueryTargetWindowSpec> GetCurrentQueryTarget() const { return CurrentQueryTarget; }
	void SetCurrentQueryTarget(TSharedPtr<Insights::FQueryTargetWindowSpec> InTarget) { CurrentQueryTarget = InTarget; }
	void AddQueryTarget(TSharedPtr<Insights::FQueryTargetWindowSpec> InPtr);
	void RemoveQueryTarget(TSharedPtr<Insights::FQueryTargetWindowSpec> InPtr);

private:
	void SyncTrackers();
	int32 GetNextMemoryGraphTrackOrder();
	TSharedPtr<FMemoryGraphTrack> CreateGraphTrack(const Insights::FReportTypeGraphConfig& ReportTypeGraphConfig, bool bIsPlatformTracker);
	void InitMemoryRules();
	void OnMemoryRuleChanged();

private:
	TSharedPtr<STimingView> TimingView;

	Insights::FMemoryTagList TagList;

	TArray<TSharedPtr<Insights::FMemoryTracker>> Trackers;
	TSharedPtr<Insights::FMemoryTracker> DefaultTracker;
	TSharedPtr<Insights::FMemoryTracker> PlatformTracker;

	TSharedPtr<FMemoryGraphTrack> MainGraphTrack; // the Main Memory Graph track; also hosts the Total Allocated Memory series
	TSharedPtr<FMemoryGraphTrack> LiveAllocsGraphTrack; // the graph track for the Live Allocation Count series
	TSharedPtr<FMemoryGraphTrack> AllocFreeGraphTrack; // the graph track for the Alloc Event Count and the Free Event Count series
	TSet<TSharedPtr<FMemoryGraphTrack>> AllTracks;

	EMemoryTrackHeightMode TrackHeightMode;

	bool bShowHideAllMemoryTracks;

	TBitArray<> CreatedDefaultTracks;

	TArray<TSharedPtr<Insights::FMemoryRuleSpec>> MemoryRules;
	TSharedPtr<Insights::FMemoryRuleSpec> CurrentMemoryRule;

	TSharedPtr<Insights::FQueryTargetWindowSpec> CurrentQueryTarget;
	TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>> QueryTargetSpecs;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
