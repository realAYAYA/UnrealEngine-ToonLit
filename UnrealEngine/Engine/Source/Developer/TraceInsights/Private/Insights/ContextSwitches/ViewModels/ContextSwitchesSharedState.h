// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

// Insights
#include "Insights/ITimingViewExtender.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FBaseTimingTrack;
class FMenuBuilder;
class FThreadTimingTrack;
class ITimingEvent;
class STimingView;

namespace TraceServices
{
	class IAnalysisSession;
}

namespace Insights
{

class FCpuCoreTimingTrack;
class FContextSwitchesTimingTrack;
class ITimingViewSession;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FContextSwitchesStateCommands : public TCommands<FContextSwitchesStateCommands>
{
public:
	FContextSwitchesStateCommands();
	virtual ~FContextSwitchesStateCommands();
	virtual void RegisterCommands() override;

	// Commands for the Tracks Filter menu.
	TSharedPtr<FUICommandInfo> Command_ShowCoreTracks;
	TSharedPtr<FUICommandInfo> Command_ShowContextSwitches;
	TSharedPtr<FUICommandInfo> Command_ShowOverlays;
	TSharedPtr<FUICommandInfo> Command_ShowExtendedLines;
	TSharedPtr<FUICommandInfo> Command_ShowNonTargetProcessEvents;

	// Commands for a Cpu Core track (context menu).
	TSharedPtr<FUICommandInfo> Command_NavigateToCpuThreadEvent;
	TSharedPtr<FUICommandInfo> Command_DockCpuThreadTrackToBottom;

	// Commands for a Cpu Thread track (context menu).
	TSharedPtr<FUICommandInfo> Command_NavigateToCpuCoreEvent;
	TSharedPtr<FUICommandInfo> Command_DockCpuCoreTrackToTop;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FContextSwitchesSharedState : public Insights::ITimingViewExtender, public TSharedFromThis<FContextSwitchesSharedState>
{
public:
	FContextSwitchesSharedState(STimingView* InTimingView);
	virtual ~FContextSwitchesSharedState() = default;

	//////////////////////////////////////////////////
	// ITimingViewExtender interface

	virtual void OnBeginSession(Insights::ITimingViewSession& InSession) override;
	virtual void OnEndSession(Insights::ITimingViewSession& InSession) override;
	virtual void Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	virtual void ExtendCpuTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;
	virtual void AddQuickFindFilters(TSharedPtr<class FFilterConfigurator> FilterConfigurator) override;

	//////////////////////////////////////////////////

	static TSharedPtr<STimingView> GetTimingView();

	bool AreContextSwitchesAvailable() const;

	bool AreCoreTracksVisible() const { return bAreCoreTracksVisible; }
	void ShowCoreTracks() { SetCoreTracksVisible(true); }
	void HideCoreTracks() { SetCoreTracksVisible(false); }
	void ToggleCoreTracks() { SetCoreTracksVisible(!bAreCoreTracksVisible); }
	void SetCoreTracksVisible(bool bOnOff);

	bool AreContextSwitchesVisible() const { return bAreContextSwitchesVisible; }
	void ShowContextSwitches() { SetContextSwitchesVisible(true); }
	void HideContextSwitches() { SetContextSwitchesVisible(false); }
	void ToggleContextSwitches() { SetContextSwitchesVisible(!bAreContextSwitchesVisible); }
	void SetContextSwitchesVisible(bool bOnOff);

	bool AreOverlaysVisible() const { return bAreOverlaysVisible; }
	void ShowOverlays() { SetOverlaysVisible(true); }
	void HideOverlays() { SetOverlaysVisible(false); }
	void ToggleOverlays() { SetOverlaysVisible(!bAreOverlaysVisible); }
	void SetOverlaysVisible(bool bOnOff);

	bool AreExtendedLinesVisible() const { return bAreExtendedLinesVisible; }
	void ShowExtendedLines() { SetExtendedLinesVisible(true); }
	void HideExtendedLines() { SetExtendedLinesVisible(false); }
	void ToggleExtendedLines() { SetExtendedLinesVisible(!bAreExtendedLinesVisible); }
	void SetExtendedLinesVisible(bool bOnOff);

	bool AreNonTargetProcessEventsVisible() const { return bAreNonTargetProcessEventsVisible; }
	void ShowNonTargetProcessEvents() { SetNonTargetProcessEventsVisible(true); }
	void HideNonTargetProcessEvents() { SetNonTargetProcessEventsVisible(false); }
	void ToggleNonTargetProcessEvents() { SetNonTargetProcessEventsVisible(!bAreNonTargetProcessEventsVisible); }
	void SetNonTargetProcessEventsVisible(bool bOnOff);

	void SetTargetTimingEvent(const TSharedPtr<const ITimingEvent> InEvent)
	{
		TargetTimingEvent = InEvent;
	}

	void AddCommands();

	void GetThreadInfo(uint32 InSystemThreadId, uint32& OutThreadId, const TCHAR*& OutThreadName) const;
	TSharedPtr<FThreadTimingTrack> GetThreadTimingTrack(uint32 ThreadId) const;
	TSharedPtr<Insights::FCpuCoreTimingTrack> GetCpuCoreTimingTrack(uint32 CoreNumber) const;

private:
	void AddCoreTracks();
	void RemoveCoreTracks();

	void AddContextSwitchesChildTracks();
	void RemoveContextSwitchesChildTracks();

	void BuildSubMenu(FMenuBuilder& InMenuBuilder);

	void Command_ShowCoreTracks_Execute() { ToggleCoreTracks(); }
	bool Command_ShowCoreTracks_CanExecute() const { return AreContextSwitchesAvailable(); }
	bool Command_ShowCoreTracks_IsChecked() const { return AreCoreTracksVisible(); }

	void Command_ShowContextSwitches_Execute() { ToggleContextSwitches(); }
	bool Command_ShowContextSwitches_CanExecute() const { return AreContextSwitchesAvailable(); }
	bool Command_ShowContextSwitches_IsChecked() const { return AreContextSwitchesVisible(); }

	void Command_ShowOverlays_Execute() { ToggleOverlays(); }
	bool Command_ShowOverlays_CanExecute() const { return AreContextSwitchesAvailable() && AreContextSwitchesVisible(); }
	bool Command_ShowOverlays_IsChecked() const { return AreOverlaysVisible(); }

	void Command_ShowExtendedLines_Execute() { ToggleExtendedLines(); }
	bool Command_ShowExtendedLines_CanExecute() const { return AreContextSwitchesAvailable() && AreContextSwitchesVisible(); }
	bool Command_ShowExtendedLines_IsChecked() const { return AreExtendedLinesVisible(); }

	void Command_ShowNonTargetProcessEvents_Execute() { ToggleNonTargetProcessEvents(); }
	bool Command_ShowNonTargetProcessEvents_CanExecute() const { return AreContextSwitchesAvailable() && AreCoreTracksVisible(); }
	bool Command_ShowNonTargetProcessEvents_IsChecked() const { return AreNonTargetProcessEventsVisible(); }

	bool IsValidCpuCoreEventSelected() const;
	bool IsValidContextSwitchEventSelected() const;

	void Command_NavigateToCpuThreadEvent_Execute();
	bool Command_NavigateToCpuThreadEvent_CanExecute() const;

	void Command_DockCpuThreadTrackToBottom_Execute();
	bool Command_DockCpuThreadTrackToBottom_CanExecute() const { return AreContextSwitchesAvailable() && AreContextSwitchesVisible() && IsValidCpuCoreEventSelected(); }

	void Command_NavigateToCpuCoreEvent_Execute();
	bool Command_NavigateToCpuCoreEvent_CanExecute() const { return AreContextSwitchesAvailable() && AreCoreTracksVisible() && IsValidContextSwitchEventSelected(); }

	void Command_DockCpuCoreTrackToTop_Execute();
	bool Command_DockCpuCoreTrackToTop_CanExecute() const { return AreContextSwitchesAvailable() && AreCoreTracksVisible() && IsValidContextSwitchEventSelected(); }

	void PopulateCoreEventNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions);

private:
	TMap<uint32, TSharedPtr<FCpuCoreTimingTrack>> CpuCoreTimingTracks;
	TMap<uint32, TSharedPtr<FContextSwitchesTimingTrack>> ContextSwitchesTimingTracks;

	Insights::ITimingViewSession* TimingViewSession;

	uint64 ThreadsSerial;
	uint64 CpuCoresSerial;

	bool bAreCoreTracksVisible;
	bool bAreContextSwitchesVisible;
	bool bAreOverlaysVisible;
	bool bAreExtendedLinesVisible;
	bool bAreNonTargetProcessEventsVisible;

	bool bSyncWithProviders;

	TSharedPtr<const ITimingEvent> TargetTimingEvent;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
