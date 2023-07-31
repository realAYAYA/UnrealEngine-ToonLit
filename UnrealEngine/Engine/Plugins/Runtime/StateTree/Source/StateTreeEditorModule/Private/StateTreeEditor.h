// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IStateTreeEditor.h"
#include "StateTreeViewModel.h"

class IDetailsView;
class UStateTree;
class FStateTreeViewModel;

namespace UE::StateTree::Editor
{
	// Validates asset state
	void ValidateAsset(UStateTree& StateTree);

	// Calculates editor data hash of the asset. 
	uint32 CalcAssetHash(const UStateTree& StateTree);
 
} // UE::StateTree::Editor


class FStateTreeEditor : public IStateTreeEditor, public FSelfRegisteringEditorUndoClient, public FGCObject
{
public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* StateTree);

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	//~ End IToolkit Interface

	void OnAssetFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	void OnSelectionFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FStateTreeEditor");
	}

protected:
	/** Called when "Save" is clicked for this asset */
	virtual void SaveAsset_Execute() override;

private:

	void HandleMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken);
	void HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UStateTreeState>>& SelectedStates);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab_StateTreeView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectionDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_StateTreeStatistics(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CompilerResults(const FSpawnTabArgs& Args);

	void BindCommands();
	void RegisterToolbar();

	void Compile();
	bool CanCompile() const;
	FSlateIcon GetCompileStatusImage() const;

	void UpdateAsset();

	void HandleModelAssetChanged();
	void OnIdentifierChanged(const UStateTree& StateTree);
	void OnSchemaChanged(const UStateTree& StateTree);
	void OnParametersChanged(const UStateTree& StateTree);
	void OnStateParametersChanged(const UStateTree& StateTree, const FGuid StateID);

	FText GetStatisticsText() const;

	/* State Tree being edited */
	UStateTree* StateTree = nullptr;

	uint32 EditorDataHash = 0;
	bool bLastCompileSucceeded = true;
	
	/** Selection Property View */
	TSharedPtr<class IDetailsView> SelectionDetailsView;

	/** Asset Property View */
	TSharedPtr<class IDetailsView> AssetDetailsView;

	/** Tree View */
	TSharedPtr<class SStateTreeView> StateTreeView;

	/** Compiler Results log */
	TSharedPtr<class SWidget> CompilerResults;
	TSharedPtr<class IMessageLogListing> CompilerResultsListing;
	
	TSharedPtr<FStateTreeViewModel> StateTreeViewModel;

	static const FName StateTreeViewTabId;
	static const FName SelectionDetailsTabId;
	static const FName AssetDetailsTabId;
	static const FName StateTreeStatisticsTabId;
	static const FName CompilerResultsTabId;
};
