// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorMode.h"
#include "GeometryBase.h"
#include "Delegates/IDelegateInstance.h"
#include "ChaosClothAsset/ClothPatternVertexType.h"
#include "ClothEditorMode.generated.h"

PREDECLARE_GEOMETRY(class FDynamicMesh3);

class FEditorViewportClient;
class FAssetEditorModeManager;
class FToolCommandChange;
class UMeshElementsVisualizer;
class UPreviewMesh;
class UToolTarget;
class FToolTargetTypeRequirements;
class UWorld;
class UInteractiveToolPropertySet; 
class UMeshOpPreviewWithBackgroundCompute;
class UClothToolViewportButtonsAPI;
class UDynamicMeshComponent;
class UChaosClothComponent;
class FEditorViewportClient;
class FViewport;
class UDataflow;
class UDataflowComponent;
class SDataflowGraphEditor;
struct FManagedArrayCollection;
namespace UE::Chaos::ClothAsset
{
class FChaosClothPreviewScene;
class FChaosClothAssetEditorModeToolkit;
class FChaosClothAssetEditorToolkit;
class FChaosClothEditorRestSpaceViewportClient;
}
class IChaosClothAssetEditorToolBuilder;
class UEdGraphNode;
class UPreviewGeometry;

/**
 * The cloth editor mode is the mode used in the cloth asset editor. It holds most of the inter-tool state.
 * We put things in a mode instead of directly into the asset editor in case we want to someday use the mode
 * in multiple asset editors.
 */
UCLASS(Transient)
class CHAOSCLOTHASSETEDITOR_API UChaosClothAssetEditorMode final : public UBaseCharacterFXEditorMode
{
	GENERATED_BODY()

public:

	const static FEditorModeID EM_ChaosClothAssetEditorModeId;

	UChaosClothAssetEditorMode();

	// Bounding box for selected rest space mesh components
	FBox SelectionBoundingBox() const;

	// Bounding box for sim space meshes
	FBox PreviewBoundingBox() const;

	void SetConstructionViewMode(UE::Chaos::ClothAsset::EClothPatternVertexType InMode);
	UE::Chaos::ClothAsset::EClothPatternVertexType GetConstructionViewMode() const;
	bool CanChangeConstructionViewModeTo(UE::Chaos::ClothAsset::EClothPatternVertexType NewViewMode) const;

	void ToggleConstructionViewWireframe();
	bool CanSetConstructionViewWireframeActive() const;
	bool IsConstructionViewWireframeActive() const
	{
		return bConstructionViewWireframe;
	}

	void ToggleConstructionViewSeams();
	bool CanSetConstructionViewSeamsActive() const;
	bool IsConstructionViewSeamsActive() const
	{
		return bConstructionViewSeamsVisible;
	}

	void ToggleConstructionViewSeamsCollapse();
	bool CanSetConstructionViewSeamsCollapse() const;
	bool IsConstructionViewSeamsCollapseActive() const
	{
		return bConstructionViewSeamsCollapse;
	}

	void TogglePatternColor();
	bool CanSetPatternColor() const;
	bool IsPatternColorActive() const
	{
		return bPatternColors;
	}

	void ToggleMeshStats();
	bool CanSetMeshStats() const;
	bool IsMeshStatsActive() const
	{
		return bMeshStats;
	}

	// Simulation controls
	void SoftResetSimulation();
	void HardResetSimulation();
	void SuspendSimulation();
	void ResumeSimulation();
	bool IsSimulationSuspended() const;
	void SetEnableSimulation(bool bEnabled);
	bool IsSimulationEnabled() const;

	int32 GetConstructionViewTriangleCount() const;
	int32 GetConstructionViewVertexCount() const;

	// LODIndex == INDEX_NONE is LOD Auto
	void SetLODModel(int32 LODIndex);
	bool IsLODModelSelected(int32 LODIndex) const;
	int32 GetLODModel() const;
	int32 GetNumLODs() const;

	UDataflowComponent* GetDataflowComponent() const;

	TObjectPtr<UEditorInteractiveToolsContext> GetActiveToolsContext()
	{
		return ActiveToolsContext;
	}

private:

	friend class UE::Chaos::ClothAsset::FChaosClothAssetEditorToolkit;
	friend class UE::Chaos::ClothAsset::FChaosClothAssetEditorModeToolkit;

	// UEdMode
	virtual void Enter() final;
	virtual void Exit() override;
	virtual void ModeTick(float DeltaTime) override;
	virtual bool ShouldToolStartBeAllowed(const FString& ToolIdentifier) const override;
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void CreateToolkit() override;
	virtual void BindCommands() override;

	// (We don't actually override MouseEnter, etc, because things get forwarded to the input
	// router via FEditorModeTools, and we don't have any additional input handling to do at the mode level.)

	// UBaseCharacterFXEditorMode
	virtual void AddToolTargetFactories() override;
	virtual void RegisterTools() override;
	virtual void CreateToolTargets(const TArray<TObjectPtr<UObject>>& AssetsIn) override;
	virtual void InitializeTargets(const TArray<TObjectPtr<UObject>>& AssetsIn) override;

	// Gets the tool target requirements for the mode. The resulting targets undergo further processing
	// to turn them into the input objects that tools get (since these need preview meshes, etc).
	static const FToolTargetTypeRequirements& GetToolTargetRequirements();

	// Use this function to register tools rather than UEdMode::RegisterTool() because we need to specify the ToolsContext
	void RegisterClothTool(TSharedPtr<FUICommandInfo> UICommand, 
		FString ToolIdentifier, 
		UInteractiveToolBuilder* Builder,
		const IChaosClothAssetEditorToolBuilder* ClothToolBuilder,
		UEditorInteractiveToolsContext* UseToolsContext, 
		EToolsContextScope ToolScope = EToolsContextScope::Default);

	void RegisterAddNodeCommand(TSharedPtr<FUICommandInfo> AddNodeCommand, const FName& NewNodeType, TSharedPtr<FUICommandInfo> StartToolCommand);

	// Register the set of tools that operate on objects in the 3D preview world 
	void RegisterPreviewTools();

	void SetPreviewScene(UE::Chaos::ClothAsset::FChaosClothPreviewScene* PreviewScene);

	// Bounding box for rest space meshes
	virtual FBox SceneBoundingBox() const override;

	void SetRestSpaceViewportClient(TWeakPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient, ESPMode::ThreadSafe> ViewportClient);
	void RefocusRestSpaceViewportClient();
	void FirstTimeFocusRestSpaceViewport();

	// intended to be called by the toolkit when selected node in the Dataflow graph changes
	void SetSelectedClothCollection(TSharedPtr<FManagedArrayCollection> Collection, TSharedPtr<FManagedArrayCollection> InputCollection = nullptr);

	// gets the currently selected cloth collection, as specified by the toolkit
	TSharedPtr<FManagedArrayCollection> GetClothCollection();
	TSharedPtr<FManagedArrayCollection> GetInputClothCollection();

	void SetDataflowGraphEditor(TSharedPtr<SDataflowGraphEditor> InGraphEditor);
	
	void StartToolForSelectedNode(const UObject* SelectedNode);
	void OnDataflowNodeDeleted(const TSet<UObject*>& DeletedNodes);

	/**
	* Return the single selected node in the Dataflow Graph Editor only if it has an output of the specified type
	* If there is not a single node selected, or if it does not have the specified output, return null
	*/
	UEdGraphNode* GetSingleSelectedNodeWithOutputType(const FName& SelectedNodeOutputTypeName) const;

	/**
	 * Create a node with the specified type in the graph
	*/
	UEdGraphNode* CreateNewNode(const FName& NewNodeTypeName);

	/** Create a node with the specified type, then connect it to the output of the specified UpstreamNode.
	* If the specified output of the upstream node is already connected to another node downstream, we first break
	* that connecttion, then insert the new node along the previous connection.
	* We want to turn this:
	*
	* [UpstreamNode] ----> [DownstreamNode(s)]
	*
	* to this:
	*
	* [UpstreamNode] ----> [NewNode] ----> [DownstreamNode(s)]
	*
	*
	* @param NewNodeTypeName The type of node to create, by name
	* @param UpstreamNode Node to connect the new node to
	* @param ConnectionTypeName The type of output of the upstream node to connect our new node to
	* @param NewNodeConnectionName The name of the input/output connection on our new node that will be connected
	* @return The newly-created node
	*/
	UEdGraphNode* CreateAndConnectNewNode(const FName& NewNodeTypeName,	UEdGraphNode& UpstreamNode,	const FName& ConnectionTypeName, const FName& NewNodeConnectionName);


	void InitializeContextObject();
	void DeleteContextObject();

	bool IsComponentSelected(const UPrimitiveComponent* InComponent);

	// Rest space wireframe. They have to get ticked to be able to respond to setting changes. 
	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizer> WireframeDraw = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> ClothSeamDraw = nullptr;


	// Preview Scene, here largely for convenience to avoid having to pass it around functions. Owned by the ClothEditorToolkit.
	UE::Chaos::ClothAsset::FChaosClothPreviewScene* PreviewScene = nullptr;

	// Mode-level property objects (visible or not) that get ticked.
	UPROPERTY()
	TArray<TObjectPtr<UInteractiveToolPropertySet>> PropertyObjectsToTick;

	// Rest space editable mesh
	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent = nullptr;

	// Actor required for hit testing DynamicMeshComponent
	UPROPERTY()
	TObjectPtr<AActor> DynamicMeshComponentParentActor = nullptr;

	TWeakPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient, ESPMode::ThreadSafe> RestSpaceViewportClient;

	// The first time we get a valid mesh, refocus the camera on it
	bool bFirstValid2DMesh = true;
	bool bFirstValid3DMesh = true;

	// Whether the rest space viewport should focus on the rest space mesh on the next tick
	bool bShouldFocusRestSpaceView = true;

	void RestSpaceViewportResized(FViewport* RestspaceViewport, uint32 Unused);

	UE::Chaos::ClothAsset::EClothPatternVertexType ConstructionViewMode;

	// The Construction view mode that was active before starting the current tool. When the tool ends, restore this view mode if bShouldRestoreSavedConstructionViewMode is true.
	UE::Chaos::ClothAsset::EClothPatternVertexType SavedConstructionViewMode;

	// Whether we should restore the previous view mode when a tool ends
	bool bShouldRestoreSavedConstructionViewMode = false;

	// Dataflow node type whose corresponding tool should be started on the next Tick
	FName NodeTypeForPendingToolStart;

	bool bConstructionViewWireframe = false;
	bool bShouldRestoreConstructionViewWireframe = false;

	bool bConstructionViewSeamsVisible = false;
	bool bShouldRestoreConstructionViewSeams = false;
	bool bConstructionViewSeamsCollapse = false;
	void InitializeSeamDraw();

	bool bPatternColors = false;
	bool bMeshStats = false;

	// Create dynamic mesh components from the cloth component's rest space info
	void ReinitializeDynamicMeshComponents();

	// Simulation controls
	bool bShouldResetSimulation = false;
	bool bHardReset = false;
	bool bShouldClearTeleportFlag = false;

	UPROPERTY()
	TObjectPtr<UDataflowComponent> DataflowComponent = nullptr;

	TWeakObjectPtr<UDataflow> DataflowGraph = nullptr;

	TWeakPtr<SDataflowGraphEditor> DataflowGraphEditor;

	UPROPERTY()
	TObjectPtr<UEditorInteractiveToolsContext> ActiveToolsContext = nullptr;

	TSharedPtr<FManagedArrayCollection> SelectedClothCollection = nullptr;
	TSharedPtr<FManagedArrayCollection> SelectedInputClothCollection = nullptr;

	// Correspondence between node types and commands to launch tools
	TMap<FName, TSharedPtr<const FUICommandInfo>> NodeTypeToToolCommandMap;
};

