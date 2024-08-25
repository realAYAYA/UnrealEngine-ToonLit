// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorToolkit.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Misc/NotifyHook.h"
#include "GraphEditor.h"
#include "TickableEditorObject.h"
#include "Dataflow/DataflowSelectionView.h"
#include "Dataflow/DataflowCollectionSpreadSheet.h"
#include "Dataflow/DataflowEditorViewport.h"

class FEditorViewportTabContent;
class IDetailsView;
class FTabManager;
class IStructureDetailsView;
class IToolkitHost;
class UDataflow;
class USkeletalMesh;
class SDataflowGraphEditor;
class FDataflowConstructionScene;
class UDataflowBaseContent;
class FDataflowSimulationScene;

namespace Dataflow
{
	class DATAFLOWEDITOR_API FAssetContext : public TEngineContext<FContextSingle>
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(TEngineContext<FContextSingle>, FAssetContext); 

		FAssetContext(UObject* InOwner, UDataflow* InGraph, FTimestamp InTimestamp)
			: Super(InOwner, InGraph, InTimestamp)
		{}
	};
}

class DATAFLOWEDITOR_API FDataflowEditorToolkit final : public FBaseCharacterFXEditorToolkit, public FTickableEditorObject, public FNotifyHook
{
	using FBaseCharacterFXEditorToolkit::ObjectScene;

public:

	explicit FDataflowEditorToolkit(UAssetEditor* InOwningAssetEditor);
	~FDataflowEditorToolkit();

	static bool CanOpenDataflowEditor(UObject* ObjectToEdit);
	static bool HasDataflowAsset(UObject* ObjectToEdit);
	static UDataflow* GetDataflowAsset(UObject* ObjectToEdit);
	static const UDataflow* GetDataflowAsset(const UObject* ObjectToEdit);

	/** Dataflow Content Access */
	const TObjectPtr<UDataflowBaseContent> GetDataflowContent() const;
	TObjectPtr<UDataflowBaseContent> GetDataflowContent();

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Dataflow preview scenes accessor */
	const TSharedPtr<FDataflowSimulationScene>& GetSimulationScene() const {return SimulationScene;}

protected:

	// List of dataflow actions callbacks
	void OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage);
	void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode);
	void OnNodeSelectionChanged(const TSet<UObject*>& NewSelection);
	void OnNodeDeleted(const TSet<UObject*>& NewSelection);
	void OnNodeSingleClicked(UObject* ClickedNode) const;
	void OnAssetPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	
	// Callback to remove the closed one from the listener views
	void OnTabClosed(TSharedRef<SDockTab> Tab);
	
private:
	
	// Spawning of all the additional tabs (viewport,details ones are coming from the base asset toolkit)
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Skeletal(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectionView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CollectionSpreadSheet(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SimulationViewport(const FSpawnTabArgs& Args);
	
	// FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	// FBaseCharacterFXEditorToolkit interface
	virtual FEditorModeID GetEditorModeId() const override;
	virtual void InitializeEdMode(UBaseCharacterFXEditorMode* EdMode) override;
	virtual void CreateEditorModeUILayer() override;

	// FAssetEditorToolkit interface
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void PostInitAssetEditor() override;
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	// FBaseAssetToolkit interface
	virtual void CreateWidgets() override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void CreateEditorModeManager() override;

	// List of all the tab names ids that will be used to identify the editor widgets
	static const FName GraphCanvasTabId;
	static const FName NodeDetailsTabId;
	static const FName SkeletalTabId;
	static const FName SelectionViewTabId_1;
	static const FName SelectionViewTabId_2;
	static const FName SelectionViewTabId_3;
	static const FName SelectionViewTabId_4;
	static const FName CollectionSpreadSheetTabId_1;
	static const FName CollectionSpreadSheetTabId_2;
	static const FName CollectionSpreadSheetTabId_3;
	static const FName CollectionSpreadSheetTabId_4;
	static const FName SimulationViewportTabId;

	// List of all the widgets shared ptr that will be built in the editor
	TSharedPtr<SDataflowEditorViewport> DataflowEditorViewport;
	TSharedPtr<SDataflowEditorViewport> DataflowSimulationViewport;
	TSharedPtr<SDataflowGraphEditor> GraphEditor;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<class ISkeletonTree> SkeletalEditor;
	TSharedPtr<IDetailsView> AssetDetailsEditor;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_1;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_2;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_3;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_4;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_1;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_2;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_3;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_4;

	// Utility factory functions to build the widgets
	TSharedRef<SDataflowGraphEditor> CreateGraphEditorWidget(UDataflow* ObjectToEdit, TSharedPtr<IStructureDetailsView> PropertiesEditor);
    TSharedPtr<IDetailsView> CreateAssetDetailsEditorWidget(UObject* ObjectToEdit);
    TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);
	TSharedPtr<ISkeletonTree> CreateSkeletalEditorWidget();

	/** Create the simulation viewport client */
	void CreateSimulationViewportClient();

	// List of editor commands used  for the dataflow asset
	TSharedPtr<FUICommandList> GraphEditorCommands;

	// List of selection view / collection spreadsheet widgets that are listening to any changed in the graph
	TArray<IDataflowViewListener*> ViewListeners;

	// Graph delegates used to update the UI
	FDelegateHandle OnSelectionChangedMulticastDelegateHandle;
    FDelegateHandle OnNodeDeletedMulticastDelegateHandle;
    FDelegateHandle OnFinishedChangingPropertiesDelegateHandle;
    FDelegateHandle OnFinishedChangingAssetPropertiesDelegateHandle;

	// The currently selected set of dataflow nodes. 
	UPROPERTY()
	TSet<UObject*> SelectedDataflowNodes;

	// The most recently selected dataflow node.
	UPROPERTY()
	TObjectPtr<UDataflowEdNode> PrimarySelection;
	
	/** PreviewScene showing the objects being simulated */
	TSharedPtr<FDataflowSimulationScene> SimulationScene;

	/** The editor mode manager used by the simulation preview scene */
	TSharedPtr<FEditorModeTools> SimulationModeManager;

	/** Simulation tab content */
	TSharedPtr<class FEditorViewportTabContent> SimulationTabContent;

	/** Simulation viewport delegate */
	AssetEditorViewportFactoryFunction SimulationViewportDelegate;

	/** Simulation Viewport client */
	TSharedPtr<FDataflowEditorViewportClient> SimulationViewportClient;
};
