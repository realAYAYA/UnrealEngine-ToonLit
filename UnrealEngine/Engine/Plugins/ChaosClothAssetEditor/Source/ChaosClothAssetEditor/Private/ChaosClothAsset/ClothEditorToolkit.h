// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorToolkit.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "ChaosClothAsset/ClothEditorPreviewScene.h"

template<typename T> class SComboBox;
class SClothCollectionOutliner;
class UDataflow;
class UChaosClothAsset;
class SGraphEditor;
class SDataflowGraphEditor;
class IStructureDetailsView;
class UEdGraphNode;
class UChaosClothComponent;
class SChaosClothAssetEditorRestSpaceViewport;
class SChaosClothAssetEditor3DViewport;

namespace UE::Chaos::ClothAsset
{
class FClothEditorSimulationVisualization;
class FChaosClothAssetEditor3DViewportClient;
}

namespace Dataflow
{
	class CHAOSCLOTHASSETEDITOR_API FClothAssetDataflowContext final : public TEngineContext<FContextSingle>
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(TEngineContext<FContextSingle>, FClothAssetDataflowContext);

		FClothAssetDataflowContext(UObject* InOwner, UDataflow* InGraph, FTimestamp InTimestamp)
			: Super(InOwner, InGraph, InTimestamp)
		{}
	};
}

namespace UE::Chaos::ClothAsset
{
/**
 * The toolkit is supposed to act as the UI manager for the asset editor. It's responsible
 * for setting up viewports and most toolbars, except for the internals of the mode panel.
 * However, because the toolkit also sets up the mode manager, and much of the important
 * state is held in the UChaosClothAssetEditorMode managed by the mode manager, the toolkit also ends up
 * initializing the Cloth mode.
 * Thus, the FChaosClothAssetEditorToolkit ends up being the central place for the Cloth Asset Editor setup.
 */
class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditorToolkit final : public FBaseCharacterFXEditorToolkit, public FTickableEditorObject
{
public:

	explicit FChaosClothAssetEditorToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FChaosClothAssetEditorToolkit();

	TSharedPtr<Dataflow::FEngineContext> GetDataflowContext() const;
	const UDataflow* GetDataflow() const;

private:

	static const FName ClothPreviewTabID;
	static const FName OutlinerTabID;
	static const FName PreviewSceneDetailsTabID;
	static const FName SimulationVisualizationTabID;

	// FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	// FBaseCharacterFXEditorToolkit
	virtual FEditorModeID GetEditorModeId() const override;
	virtual void InitializeEdMode(UBaseCharacterFXEditorMode* EdMode) override;
	virtual void CreateEditorModeUILayer() override;

	// FBaseAssetToolkit
	virtual void CreateWidgets() override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;

	// FAssetEditorToolkit
	virtual void AddViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;
	virtual void RemoveViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;

	virtual void PostInitAssetEditor() override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;
	virtual bool ShouldReopenEditorForSavedAsset(const UObject* Asset) const override;
	virtual void OnAssetsSavedAs(const TArray<UObject*>& SavedObjects) override;

	// IAssetEditorInstance
	virtual bool IsPrimaryEditor() const override { return true; };

	// IToolkit
	virtual FText GetToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	// Return the cloth asset held by the Cloth Editor
	UChaosClothAsset* GetAsset() const;
	UDataflow* GetDataflow();
	
	TSharedRef<SDockTab> SpawnTab_ClothPreview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Outliner(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSceneDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SimulationVisualization(const FSpawnTabArgs& Args);

	void InitDetailsViewPanel();
	void OnFinishedChangingAssetProperties(const FPropertyChangedEvent&);

	void OnClothAssetChanged();
	void InvalidateViews();

	// Dataflow
	UE_DEPRECATED(5.4, "Use EvaluateNode(FDataflowNode*, bool) instead.")
	void EvaluateNode(FDataflowNode* Node, FDataflowOutput* Out) { EvaluateNode(Node); }
	void EvaluateNode(FDataflowNode* Node, bool bForceOperation = true);
	TSharedRef<SDataflowGraphEditor> CreateGraphEditorWidget();
	void ReinitializeGraphEditorWidget();
	TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);

	TSharedPtr<FManagedArrayCollection> GetClothCollectionIfPossible(const TSharedPtr<FDataflowNode> InDataflowNode, const TSharedPtr<Dataflow::FEngineContext> Context);

	TSharedPtr<FManagedArrayCollection> GetInputClothCollectionIfPossible(const TSharedPtr<FDataflowNode> InDataflowNode, const TSharedPtr<Dataflow::FEngineContext> Context);

	// DataflowEditorActions
	void OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage) const;
	void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode) const;
	void OnNodeSelectionChanged(const TSet<UObject*>& NewSelection);
	void OnNodeDeleted(const TSet<UObject*>& DeletedNodes);
	void OnNodeSingleClicked(UObject* ClickedNode) const;

	// Delegates
	void HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

	/** Scene in which the 3D sim space preview meshes live. Ownership shared with AdvancedPreviewSettingsWidget*/
	TSharedPtr<FChaosClothPreviewScene> ClothPreviewScene;

	TSharedPtr<class FEditorViewportTabContent> ClothPreviewTabContent;
	AssetEditorViewportFactoryFunction ClothPreviewViewportDelegate;
	TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothPreviewViewportClient;
	TSharedPtr<FAssetEditorModeManager> ClothPreviewEditorModeManager;
	TSharedPtr<FClothEditorSimulationVisualization> ClothEditorSimulationVisualization;

	TSharedPtr<SChaosClothAssetEditorRestSpaceViewport> RestSpaceViewportWidget;
	TSharedPtr<SChaosClothAssetEditor3DViewport> PreviewViewportWidget;

	TSharedPtr<SDockTab> PreviewSceneDockTab;
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	TSharedPtr<SDockTab> SimulationVisualizationDockTab;

	TSharedPtr<SClothCollectionOutliner> Outliner;

	// Dataflow
	TSharedPtr<Dataflow::FEngineContext> DataflowContext;
	Dataflow::FTimestamp LastDataflowNodeTimestamp = Dataflow::FTimestamp::Invalid;
	FDelegateHandle OnNodeInvalidatedDelegateHandle;
	TSharedPtr<FDataflowNode> SelectedDataflowNode;

	static const FName GraphCanvasTabId;
	TSharedPtr<SDockTab> GraphEditorTab;
	TSharedPtr<SDataflowGraphEditor> GraphEditor;

	static const FName NodeDetailsTabId;
	TSharedPtr<SDockTab> NodeDetailsTab;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;

	FDelegateHandle OnPackageReloadedDelegateHandle;
	
};
} // namespace UE::Chaos::ClothAsset
