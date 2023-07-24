// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorToolkit.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "ChaosClothAsset/ClothEditorPreviewScene.h"

class FChaosClothAssetEditor3DViewportClient;
template<typename T> class SComboBox;
class SClothCollectionOutliner;
class UDataflow;
class UChaosClothAsset;
class SGraphEditor;
class SDataflowGraphEditor;
class IStructureDetailsView;
class UEdGraphNode;
class UChaosClothComponent;

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
	virtual bool OnRequestClose() override;
	virtual void PostInitAssetEditor() override;

	// IAssetEditorInstance
	// TODO: If this returns true then the editor cannot re-open after it's closed. Figure out why.
	virtual bool IsPrimaryEditor() const override { return false; };

	// IToolkit
	virtual FText GetToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	// Return the cloth asset held by the Cloth Editor
	UChaosClothAsset* GetAsset() const;

	TSharedRef<SDockTab> SpawnTab_ClothPreview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Outliner(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSceneDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);

	void InitDetailsViewPanel();
	void OnFinishedChangingAssetProperties(const FPropertyChangedEvent&);

	/** Scene in which the 3D sim space preview meshes live. Ownership shared with AdvancedPreviewSettingsWidget*/
	TSharedPtr<FChaosClothPreviewScene> ClothPreviewScene;

	TSharedPtr<class FEditorViewportTabContent> ClothPreviewTabContent;
	AssetEditorViewportFactoryFunction ClothPreviewViewportDelegate;
	TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothPreviewViewportClient;
	TSharedPtr<FAssetEditorModeManager> ClothPreviewEditorModeManager;

	TWeakPtr<SEditorViewport> RestSpaceViewport;

	TSharedPtr<SDockTab> PreviewSceneDockTab;
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	TSharedPtr<SClothCollectionOutliner> Outliner;

	TSharedPtr<SComboBox<FName>> SelectedGroupNameComboBox;
	TArray<FName> ClothCollectionGroupNames;		// Data source for SelectedGroupNameComboBox

	//~ Begin Dataflow support
	 
	UDataflow* Dataflow = nullptr;

	void EvaluateNode(FDataflowNode* Node, FDataflowOutput* Out);

	static const FName GraphCanvasTabId;
	TSharedPtr<SDataflowGraphEditor> GraphEditor;
	TSharedRef<SDataflowGraphEditor> CreateGraphEditorWidget();
	void ReinitializeGraphEditorWidget();

	static const FName NodeDetailsTabId;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);

	FString DataflowTerminalPath = "";
	TSharedPtr<Dataflow::FEngineContext> DataflowContext;
	Dataflow::FTimestamp LastDataflowNodeTimestamp = Dataflow::FTimestamp::Invalid;

	// DataflowEditorActions
	void OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage) const;
	void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode) const;
	void OnNodeSelectionChanged(const TSet<UObject*>& NewSelection) const;

	//~ End Dataflow support

};

