// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SLiveLinkHubTabViewWithManagerBase.h"

class FLiveLinkHubRecordingController;
struct FLiveLinkSubjectKey;
class ILiveLinkClient;

/** Manages the UI logic of the Clients tab */
class SLiveLinkHubMainTabView : public SLiveLinkHubTabViewWithManagerBase
{
public:
	//~ Tab IDs
	static const FName SourcesTabId;
	static const FName SourceDetailsTabId;
	static const FName SubjectsTabId;
	static const FName PlaybackTabId;
	static const FName ClientsTabId;
	static const FName ClientDetailsTabId;

	//~ Tab Names
	static const FText SourcesTabName;
	static const FText SourceDetailsTabName;
	static const FText SubjectsTabName;
	static const FText PlaybackTabName;
	static const FText ClientsTabName;
	static const FText ClientDetailsTabName;

	SLATE_BEGIN_ARGS(SLiveLinkHubMainTabView)
	{}
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, ConstructUnderMajorTab)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ConstructUnderWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SLiveLinkHubMainTabView() override;
	
private:
	/** Create all livelink hub tabs. */
	void CreateTabs(const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FTabManager::FLayout>& InLayout, const FArguments& InArgs);

	//~ Respective tab handlers.
	TSharedRef<SDockTab> SpawnSourcesTab(const FSpawnTabArgs& InTabArgs);
	TSharedRef<SDockTab> SpawnSourcesDetailsTab(const FSpawnTabArgs& InTabArgs);
	TSharedRef<SDockTab> SpawnSubjectsTab(const FSpawnTabArgs& InTabArgs);
	TSharedRef<SDockTab> SpawnPlaybackTab(const FSpawnTabArgs& InTabArgs);
	TSharedRef<SDockTab> SpawnClientsTab(const FSpawnTabArgs& InTabArgs);
	TSharedRef<SDockTab> SpawnClientDetailsTab(const FSpawnTabArgs& InTabArgs);

	/** Handles selection change in the subjects view. */
	void OnSubjectSelectionChanged(const FLiveLinkSubjectKey& SubjectKey);

private:
	/** Holds the livelink panel controller responsible for creating sources and subjects tabs. */
	TSharedPtr<class FLiveLinkPanelController> PanelController;
};
