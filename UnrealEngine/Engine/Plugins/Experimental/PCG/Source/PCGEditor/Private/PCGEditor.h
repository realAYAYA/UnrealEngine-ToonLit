// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorModule.h"
#include "PCGSettings.h"

#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"

class FUICommandList;
class IDetailsView;
class SGraphEditor;
class SPCGEditorGraphAttributeListView;
class SPCGEditorGraphDeterminismListView;
class SPCGEditorGraphFind;
class SPCGEditorGraphNodePalette;
class SPCGEditorGraphProfilingView;
class UPCGComponent;
class UPCGEditorGraph;
class UPCGEditorGraphNodeBase;
class UPCGGraph;
class UPCGNode;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDebugObjectChanged, UPCGComponent*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedNodeChanged, UPCGNode*);

class FPCGEditor : public FAssetEditorToolkit, public FGCObject, public FSelfRegisteringEditorUndoClient
{
public:
	/** Edits the specified PCGGraph */
	void Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph);

	/** Get the PCG graph being edited */
	UPCGEditorGraph* GetPCGEditorGraph();

	/** Sets the PCG component we want to debug */
	void SetPCGComponentBeingDebugged(UPCGComponent* InPCGComponent);

	/** Gets the PCG component we are debugging */
	UPCGComponent* GetPCGComponentBeingDebugged() const { return PCGComponentBeingDebugged; }

	/** Sets the PCG node we want to inspect */
	void SetPCGNodeBeingInspected(UPCGNode* InPCGNode);

	/** Gets the PCG node we are inspecting */
	UPCGNode* GetPCGNodeBeingInspected() const { return PCGNodeBeingInspected; }

	/** Focus the graph view on a specific node */
	void JumpToNode(const UEdGraphNode* InNode);

	// ~Begin IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	// ~End IToolkit interface

	// ~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FPCGEditor");
	}
	// ~End FGCObject interface
	
	// ~Begin FEditorUndoClient interface
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// ~End FEditorUndoClient interface

	// ~Begin FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void OnClose() override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	// ~End FAssetEditorToolkit interface

	FOnDebugObjectChanged OnDebugObjectChangedDelegate;
	FOnInspectedNodeChanged OnInspectedNodeChangedDelegate;

protected:
	// ~Begin FAssetEditorToolkit interface
	/** Called when "Save" is clicked for this asset */
	virtual void SaveAsset_Execute() override;
	// ~End FAssetEditorToolkit interface

private:
	/** Register PCG specific toolbar for the editor */
	void RegisterToolbar() const;

	/** Bind commands to delegates */
	void BindCommands();

	/** Bring up the find tab */
	void OnFind();

	/** Enable/Disable automatic PCG node generation */
	void OnPauseAutomaticRegeneration_Clicked();
	/** Has the user paused automatic regeneration in the Graph Editor */
	bool IsAutomaticRegenerationPaused() const;

	/** Force a regeneration by invoking the graph notifications  */
	void OnForceGraphRegeneration_Clicked();

	/** Start inspecting the current selected node */
	void OnStartInspectNode();
	/** Stop inspecting the current inspected node */
	void OnStopInspectNode();

	/** Can determinism be tested on the selected node(s) */
	bool CanRunDeterminismNodeTest() const;
	/** Run the determinism test on the selected node(s) */
	void OnDeterminismNodeTest();

	/** Can determinism be tested on the current graph */
	bool CanRunDeterminismGraphTest() const;
	/** Run the determinism test on the current graph */
	void OnDeterminismGraphTest();

	/** Open details view for the PCG object being edited */
	void OnEditClassDefaults() const;
	/** Whether the PCG object being edited is opened in details view or not */
	bool IsEditClassDefaultsToggled() const;

	/** Whether or not an execution mode is active for the selected nodes */
	bool IsExecutionModeActive(EPCGSettingsExecutionMode InExecutionMode) const;

	/** Set execution mode for selected nodes */
	void OnSetExecutionMode(EPCGSettingsExecutionMode InExecutionMode);

	/** Select every node in the graph */
	void SelectAllNodes();
	/** Whether we can select every node */
	bool CanSelectAllNodes() const;

	/** Delete all selected nodes in the graph */
	void DeleteSelectedNodes();
	/** Whether we can delete all selected nodes */
	bool CanDeleteSelectedNodes() const;

	/** Copy all selected nodes in the graph */
	void CopySelectedNodes();
	/** Whether we can copy all selected nodes */
	bool CanCopySelectedNodes() const;

	/** Cut all selected nodes in the graph */
	void CutSelectedNodes();
	/** Whether we can cut all selected nodes */
	bool CanCutSelectedNodes() const;

	/** Paste nodes in the graph */
	void PasteNodes();
	/** Paste nodes in the graph at location*/
	void PasteNodesHere(const FVector2D& Location);
	/** Whether we can paste nodes */
	bool CanPasteNodes() const;

	/** Duplicate the currently selected nodes */
	void DuplicateNodes();
	/** Whether we are able to duplicate the currently selected nodes */
	bool CanDuplicateNodes() const;

	/** Collapse the currently selected nodes in a subgraph */
	void OnCollapseNodesInSubgraph();
	/** Whether we can collapse nodes in a subgraph */
	bool CanCollapseNodesInSubgraph() const;

	void OnAlignTop();
	void OnAlignMiddle();
	void OnAlignBottom();
	void OnAlignLeft();
	void OnAlignCenter();
	void OnAlignRight();
	void OnStraightenConnections();
	void OnDistributeNodesH();
	void OnDistributeNodesV();
	void OnCreateComment();

	/** Create new graph editor widget */
	TSharedRef<SGraphEditor> CreateGraphEditorWidget();

	/** Create new palette widget */
	TSharedRef<SPCGEditorGraphNodePalette> CreatePaletteWidget();

	/** Create new find widget */
	TSharedRef<SPCGEditorGraphFind> CreateFindWidget();

	/** Create new attributes widget */
	TSharedRef<SPCGEditorGraphAttributeListView> CreateAttributesWidget();

	/** Create a new determinism tab widget */
	TSharedRef<SPCGEditorGraphDeterminismListView> CreateDeterminismWidget();

	/** Create a new profiling tab widget */
	TSharedRef<SPCGEditorGraphProfilingView> CreateProfilingWidget();

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

	/** Called when the title of a node is changed */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/**
	 * Called when a node is double clicked
	 *
	 * @param Node - The Node that was clicked
	 */
	void OnNodeDoubleClicked(UEdGraphNode* Node);

	/**
	 * Try to jump to a given class (if allowed)
	 *
	 * @param Class - The Class to jump to
	 */
	void JumpToDefinition(const UClass* Class) const;

	/** To be called everytime we need to replicate our extra nodes to the underlying PCGGraph */
	void ReplicateExtraNodes() const;

	TSharedRef<SDockTab> SpawnTab_GraphEditor(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PropertyDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Palette(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Attributes(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Find(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Determinism(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Profiling(const FSpawnTabArgs& Args);

	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> PropertyDetailsWidget;
	TSharedPtr<SPCGEditorGraphNodePalette> PaletteWidget;
	TSharedPtr<SPCGEditorGraphFind> FindWidget;
	TSharedPtr<SPCGEditorGraphAttributeListView> AttributesWidget;
	TSharedPtr<SPCGEditorGraphDeterminismListView> DeterminismWidget;
	TSharedPtr<SPCGEditorGraphProfilingView> ProfilingWidget;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	UPCGGraph* PCGGraphBeingEdited = nullptr;
	UPCGEditorGraph* PCGEditorGraph = nullptr;

	UPCGComponent* PCGComponentBeingDebugged = nullptr;
	UPCGNode* PCGNodeBeingInspected = nullptr;
	UPCGEditorGraphNodeBase* PCGGraphNodeBeingInspected = nullptr;
};
