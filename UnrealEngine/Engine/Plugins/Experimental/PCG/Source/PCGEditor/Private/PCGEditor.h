// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "Graph/PCGStackContext.h"

#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"

class FSpawnTabArgs;
enum class ECheckBoxState : uint8;
namespace ETextCommit { enum Type : int; }

struct FPropertyAndParent;
class FUICommandList;
class IDetailsView;
class SGraphEditor;
class SPCGEditorGraphAttributeListView;
class SPCGEditorGraphDebugObjectWidget;
class SPCGEditorGraphDebugObjectTree;
class SPCGEditorGraphDeterminismListView;
class SPCGEditorGraphFind;
class SPCGEditorGraphLogView;
class SPCGEditorGraphNodePalette;
class SPCGEditorGraphProfilingView;
class UEdGraphNode;
class UPCGComponent;
class UPCGEditorGraph;
class UPCGEditorGraphNodeBase;
class UPCGGraph;
class UPCGNode;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedComponentChanged, UPCGComponent*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedStackChanged, const FPCGStack&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedNodeChanged, UPCGEditorGraphNodeBase*);

class FPCGEditor : public FAssetEditorToolkit, public FGCObject, public FSelfRegisteringEditorUndoClient
{
public:
	/** Edits the specified PCGGraph */
	void Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph);

	/** Get the PCG graph being edited */
	UPCGEditorGraph* GetPCGEditorGraph();

	/** Sets the PCG component and stack that we want to inspect */
	void SetComponentAndStackBeingInspected(UPCGComponent* InPCGComponent, const FPCGStack& InPCGStack);

	/** Gets the PCG component we are debugging */
	UPCGComponent* GetPCGComponentBeingInspected() const { return PCGComponentBeingInspected.Get(); }
	
	/** Gets the PCG stack we are inspecting */
	const FPCGStack& GetStackBeingInspected() const { return StackBeingInspected; }

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

	FOnInspectedComponentChanged OnInspectedComponentChangedDelegate;
	FOnInspectedStackChanged OnInspectedStackChangedDelegate;
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

	/** Toggle node inspection state for selected nodes */
	void OnToggleInspected();
	/** Whether we can toggle inspection of selected nodes */
	bool CanToggleInspected() const;
	/** Whether selected nodes are inspected or not */
	ECheckBoxState GetInspectedCheckState() const;
	
	/** Toggle node enabled state for selected nodes */
	void OnToggleEnabled();
	/** Whether selected nodes are enabled or not */
	ECheckBoxState GetEnabledCheckState() const;
	
	/** Toggle node debug state for selected nodes */
	void OnToggleDebug();
	/** Whether selected nodes are being debugged or not */
	ECheckBoxState GetDebugCheckState() const;

	/** Enable node debug state for selected nodes and disable for others */
	void OnDebugOnlySelected();

	/** Disable node debug state for all nodes */
	void OnDisableDebugOnAllNodes();

	/** Cancels the current execution of the selected graph */
	void OnCancelExecution_Clicked();

	/* Returns true if inspected graph is currently scheduled or executing */
	bool IsCurrentlyGenerating() const;

	/** Can determinism be tested on the selected node(s) */
	bool CanRunDeterminismNodeTest() const;
	/** Run the determinism test on the selected node(s) */
	void OnDeterminismNodeTest();

	/** Can determinism be tested on the current graph */
	bool CanRunDeterminismGraphTest() const;
	/** Run the determinism test on the current graph */
	void OnDeterminismGraphTest();

	/** Open details view for the PCG object being edited */
	void OnEditGraphSettings() const;
	/** Whether the PCG object being edited is opened in details view or not */
	bool IsEditGraphSettingsToggled() const;

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

	/** Exports node settings to assets */
	void OnExportNodes();

	/** Whether we are able to export the currently selected nodes */
	bool CanExportNodes() const;

	/** Converts instanced nodes to independent nodes */
	void OnConvertToStandaloneNodes();

	/** Whether we are able to convert the selected nodes to standalone */
	bool CanConvertToStandaloneNodes() const;

	/** Collapse the currently selected nodes in a subgraph */
	void OnCollapseNodesInSubgraph();
	/** Whether we can collapse nodes in a subgraph */
	bool CanCollapseNodesInSubgraph() const;

	/** User is attempting to add a dynamic source pin to a node */
	void OnAddDynamicInputPin();
	/** Whether the user can add a dynamic source pin to a node */
	bool CanAddDynamicInputPin() const;

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

	/** Create debug object combo box */
	TSharedRef<SPCGEditorGraphDebugObjectWidget> CreateDebugObjectWidget();

	/** Create new debug object tree widget */
	TSharedRef<SPCGEditorGraphDebugObjectTree> CreateDebugObjectTreeWidget();

	/** Create new find widget */
	TSharedRef<SPCGEditorGraphFind> CreateFindWidget();

	/** Create new attributes widget */
	TSharedRef<SPCGEditorGraphAttributeListView> CreateAttributesWidget();

	/** Create a new determinism tab widget */
	TSharedRef<SPCGEditorGraphDeterminismListView> CreateDeterminismWidget();

	/** Create a new profiling tab widget */
	TSharedRef<SPCGEditorGraphProfilingView> CreateProfilingWidget();

	/** Create a new profiling tab widget */
	TSharedRef<SPCGEditorGraphLogView> CreateLogWidget();

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

	/** Returns whether a property should be readonly (used for instances) */
	bool IsReadOnlyProperty(const FPropertyAndParent& InPropertyAndParent, IDetailsView* InDetailsView) const;

	/** Returns whether a property should be visible (used for instance vs. settings properties) */
	bool IsVisibleProperty(const FPropertyAndParent& InPropertyAndParent, IDetailsView* InDetailsView) const;

	void OnGraphGridSizesChanged(UPCGGraphInterface* InGraph);
	void OnGraphDynamicallyExecuted(UPCGGraphInterface* InGraphInterface, const TWeakObjectPtr<UPCGComponent> InSourceComponent, FPCGStack InvocationStack);

	/** Trigger any generation required to ensure debug display is up to date. */
	void UpdateDebugAfterComponentSelection(UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool bNewComponentStartedInspecting);

	/** Helper to get to the subsystem. */
	static class UPCGSubsystem* GetSubsystem();

	void OnMapChanged(UWorld* InWorld, EMapChangeType InMapChangedType);
	void OnLevelActorDeleted(AActor* InActor);

	TSharedRef<SDockTab> SpawnTab_GraphEditor(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PropertyDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Palette(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_DebugObject(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Attributes(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Find(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Determinism(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Profiling(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Log(const FSpawnTabArgs& Args);

	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> PropertyDetailsWidget;
	TSharedPtr<SPCGEditorGraphNodePalette> PaletteWidget;
	TSharedPtr<SPCGEditorGraphDebugObjectWidget> DebugObjectWidget;
	TSharedPtr<SPCGEditorGraphDebugObjectTree> DebugObjectTreeWidget;
	TSharedPtr<SPCGEditorGraphFind> FindWidget;
	TSharedPtr<SPCGEditorGraphAttributeListView> AttributesWidget;
	TSharedPtr<SPCGEditorGraphDeterminismListView> DeterminismWidget;
	TSharedPtr<SPCGEditorGraphProfilingView> ProfilingWidget;
	TSharedPtr<SPCGEditorGraphLogView> LogWidget;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	TObjectPtr<UPCGGraph> PCGGraphBeingEdited = nullptr;
	UPCGEditorGraph* PCGEditorGraph = nullptr;

	TWeakObjectPtr<UPCGComponent> PCGComponentBeingInspected;
	FPCGStack StackBeingInspected;
	UPCGEditorGraphNodeBase* PCGGraphNodeBeingInspected = nullptr;
};
