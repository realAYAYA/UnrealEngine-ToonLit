// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class FMovieGraphAssetToolkit :  public FAssetEditorToolkit
{
public:
	FMovieGraphAssetToolkit();
	virtual ~FMovieGraphAssetToolkit() override {}

	//~ Begin FAssetEditorToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void SaveAsset_Execute() override;
	virtual void OnClose() override;
	//~ End FAssetEditorToolkit interface

	//~ Begin IToolkit interface
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ End IToolkit interface

	void InitMovieGraphAssetToolkit(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, class UMovieGraphConfig* InitGraph);

private:
	TSharedRef<SDockTab> SpawnTab_RenderGraphEditor(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RenderGraphMembers(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RenderGraphDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RenderGraphActiveRenderSettings(const FSpawnTabArgs& Args);

	/** Register commands used by the editor. */
	void BindGraphCommands();

	/** Deletes the member(s) which are currently selected. */
	void DeleteSelectedMembers();

	/** Determines if the selected member(s) can be deleted. */
	bool CanDeleteSelectedMembers();

	/** Copies editor-only nodes to the underlying runtime graph so they can be restored later. */
	void PersistEditorOnlyNodes() const;

private:
	/** The details panel for the selected object(s) in the graph */
	TSharedPtr<IDetailsView> SelectedGraphObjectsDetailsWidget;

	/** The widget that contains the main node graph */
	TSharedPtr<class SMoviePipelineGraphPanel> MovieGraphWidget;

	/** The widget which encapsulates all of the content in the Members tab */
	TSharedPtr<class SMovieGraphMembersTabContent> MembersTabContent;

	/** The widget which encapsulates all of the content in the Active Render Settings tab */
	TSharedPtr<class SMovieGraphActiveRenderSettingsTabContent> ActiveRenderSettingsTabContent;
	
	/** The graph that the editor was initialized with */
	TObjectPtr<UMovieGraphConfig> InitialGraph;

	/** Flag which is set when a selection change is not triggered by the user */
	bool bIsInternalSelectionChange;

	static const FName AppIdentifier;
	static const FName GraphTabId;
	static const FName DetailsTabId;
	static const FName MembersTabId;
	static const FName ActiveRenderSettingsTabId;
};
