// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorMode.h"
#include "GeometryBase.h"
#include "Delegates/IDelegateInstance.h"
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
class FChaosClothAssetEditorModeToolkit;
class UInteractiveToolPropertySet; 
class UMeshOpPreviewWithBackgroundCompute;
class UClothToolViewportButtonsAPI;
class UDynamicMeshComponent;
class UChaosClothComponent;
class FEditorViewportClient;
class FChaosClothEditorRestSpaceViewportClient;
class FViewport;
class UDataflowComponent;
class FChaosClothPreviewScene;

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

	// Toggle between 2D pattern and 3D rest space mesh view
	void TogglePatternMode();
	bool CanTogglePatternMode() const;

	// Simulation controls
	void SoftResetSimulation();
	void HardResetSimulation();
	void SuspendSimulation();
	void ResumeSimulation();
	bool IsSimulationSuspended() const;

	UDataflowComponent* GetDataflowComponent() const;

private:

	friend class FChaosClothAssetEditorToolkit;

	// UEdMode
	virtual void Enter() final;
	virtual void Exit() override;
	virtual void ModeTick(float DeltaTime) override;
	virtual bool ShouldToolStartBeAllowed(const FString& ToolIdentifier) const override;
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void PostUndo() override;
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

	void SetPreviewScene(FChaosClothPreviewScene* PreviewScene);

	// Bounding box for rest space meshes
	virtual FBox SceneBoundingBox() const override;

	void SetRestSpaceViewportClient(TWeakPtr<FChaosClothEditorRestSpaceViewportClient, ESPMode::ThreadSafe> ViewportClient);
	void RefocusRestSpaceViewportClient();


	// Rest space wireframes. They have to get ticked to be able to respond to setting changes. 
	UPROPERTY()
	TArray<TObjectPtr<UMeshElementsVisualizer>> WireframesToTick;

	// Preview Scene, here largely for convenience to avoid having to pass it around functions. Owned by the ClothEditorToolkit.
	FChaosClothPreviewScene* PreviewScene = nullptr;

	// Mode-level property objects (visible or not) that get ticked.
	UPROPERTY()
	TArray<TObjectPtr<UInteractiveToolPropertySet>> PropertyObjectsToTick;

	// Rest space editable meshes
	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshComponent>> DynamicMeshComponents;

	// Actors required for hit testing DynamicMeshComponents
	UPROPERTY()
	TArray<TObjectPtr<AActor>> DynamicMeshComponentParentActors;

	// Map back to original asset location for each DynamicMeshComponent
	struct FDynamicMeshSourceInfo
	{
		int32 LodIndex;
		int32 PatternIndex;
	};
	TArray<FDynamicMeshSourceInfo> DynamicMeshSourceInfos;

	TWeakPtr<FChaosClothEditorRestSpaceViewportClient, ESPMode::ThreadSafe> RestSpaceViewportClient;

	// Handle to a callback triggered when the current selection changes
	FDelegateHandle SelectionModifiedEventHandle;

	// Whether to display the 2D pattern or 3D assembly in the rest space viewport
	bool bPattern2DMode = true;

	// Whether we can switch between 2D and 3D rest configuration
	bool bCanTogglePattern2DMode = true;

	// Whether the rest space viewport should focus on the rest space mesh on the next tick
	bool bShouldFocusRestSpaceView = true;
	void RestSpaceViewportResized(FViewport* RestspaceViewport, uint32 Unused);

	// Whether to combine all patterns into a single DynamicMeshComponent, or have separate components for each pattern
	// TODO: Expose this to the user
	bool bCombineAllPatterns = false;

	bool IsComponentSelected(const UPrimitiveComponent* InComponent);

	// Create dynamic mesh components from the cloth component's rest space info
	void ReinitializeDynamicMeshComponents();

	// Set up the preview simulation mesh from the given rest-space mesh
	void UpdateSimulationMeshes();

	// Simulation controls
	bool bShouldResetSimulation = false;
	bool bHardReset = false;
	bool bShouldClearTeleportFlag = false;

	UPROPERTY()
	TObjectPtr<UDataflowComponent> DataflowComponent = nullptr;
};

