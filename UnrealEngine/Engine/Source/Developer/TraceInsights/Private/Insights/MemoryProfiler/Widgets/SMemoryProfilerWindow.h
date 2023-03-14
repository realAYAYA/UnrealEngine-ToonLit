// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ITimingViewSession.h" // for Insights::ETimeChangedFlags
#include "Insights/Widgets/SMajorTabWindow.h"
#include "Insights/Widgets/SModulesView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemorySharedState;
class SMemInvestigationView;
class SMemTagTreeView;
class STimingView;

namespace Insights
{
	class FTimeMarker;
	class STableTreeView;
	class SMemAllocTableTreeView;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemoryProfilerTabs
{
	// Tab identifiers
	static const FName TimingViewID;
	static const FName MemInvestigationViewID;
	static const FName MemTagTreeViewID;
	static const FName MemAllocTableTreeViewID; // base id
	static const FName ModulesViewID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Implements the Memory Insights window. */
class SMemoryProfilerWindow : public Insights::SMajorTabWindow
{
public:
	/** Default constructor. */
	SMemoryProfilerWindow();

	/** Virtual destructor. */
	virtual ~SMemoryProfilerWindow();

	SLATE_BEGIN_ARGS(SMemoryProfilerWindow) {}
	SLATE_END_ARGS()

	virtual void Reset() override;

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);

	TSharedPtr<STimingView> GetTimingView() const { return TimingView; }
	TSharedPtr<SMemInvestigationView> GetMemInvestigationView() const { return MemInvestigationView; }
	TSharedPtr<SMemTagTreeView> GetMemTagTreeView() const { return MemTagTreeView; }

	void CloseMemAllocTableTreeTabs();
	TSharedPtr<Insights::SMemAllocTableTreeView> ShowMemAllocTableTreeViewTab();

	uint32 GetNumCustomTimeMarkers() const { return (uint32)CustomTimeMarkers.Num(); }
	const TSharedRef<Insights::FTimeMarker>& GetCustomTimeMarker(uint32 Index) const { return CustomTimeMarkers[Index]; }
	const TArray<TSharedRef<Insights::FTimeMarker>>& GetCustomTimeMarkers() const { return CustomTimeMarkers; }

	FMemorySharedState& GetSharedState() { return *SharedState; }
	const FMemorySharedState& GetSharedState() const { return *SharedState; }

	void OnMemoryRuleChanged();
	void OnTimeMarkerChanged(Insights::ETimeChangedFlags InFlags, TSharedRef<Insights::ITimeMarker> InTimeMarker);

protected:
	virtual const TCHAR* GetAnalyticsEventName() const override;
	virtual TSharedRef<FWorkspaceItem> CreateWorkspaceMenuGroup() override;
	virtual void RegisterTabSpawners() override;
	virtual TSharedRef<FTabManager::FLayout> CreateDefaultTabLayout() const override;
	virtual TSharedRef<SWidget> CreateToolbar(TSharedPtr<FExtender> Extender);

private:
	TSharedRef<SDockTab> SpawnTab_TimingView(const FSpawnTabArgs& Args);
	void OnTimingViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	TSharedRef<SDockTab> SpawnTab_MemInvestigationView(const FSpawnTabArgs& Args);
	void OnMemInvestigationViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	TSharedRef<SDockTab> SpawnTab_MemTagTreeView(const FSpawnTabArgs& Args);
	void OnMemTagTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	TSharedRef<SDockTab> SpawnTab_MemAllocTableTreeView(const FSpawnTabArgs& Args, int32 TabIndex);
	void OnMemAllocTableTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	TSharedRef<SDockTab> SpawnTab_ModulesView(const FSpawnTabArgs& Args);
	void OnModulesViewClosed(TSharedRef<SDockTab> TabBeingClosed);

	void OnTimeSelectionChanged(Insights::ETimeChangedFlags InFlags, double InStartTime, double InEndTime);

	void CreateTimingViewMarkers();
	void ResetTimingViewMarkers();
	void UpdateTimingViewMarkers();

private:
	TSharedRef<FMemorySharedState> SharedState;

	/** The Timing view (multi-track) widget */
	TSharedPtr<STimingView> TimingView;

	TArray<TSharedRef<Insights::FTimeMarker>> CustomTimeMarkers;

	/** The Memory Investigation (Allocation Queries) view widget */
	TSharedPtr<SMemInvestigationView> MemInvestigationView;

	/** The "LLM Tags" tree view widget */
	TSharedPtr<SMemTagTreeView> MemTagTreeView;

	/** The list of Allocations table tree view widgets */
	TArray<TSharedPtr<Insights::SMemAllocTableTreeView>> MemAllocTableTreeViews;

	/** The Modules view widget. */
	TSharedPtr<Insights::SModulesView> ModulesView;

	const int32 MaxMemAllocTableTreeViews = 4;
	int32 LastMemAllocTableTreeViewIndex = -1;
};
