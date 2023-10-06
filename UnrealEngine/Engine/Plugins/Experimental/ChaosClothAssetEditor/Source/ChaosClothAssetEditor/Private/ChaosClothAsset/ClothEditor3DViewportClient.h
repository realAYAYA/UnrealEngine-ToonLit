// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "GeometryCollection/ManagedArrayCollection.h"

class UChaosClothComponent;
class UChaosClothAssetEditorMode;
class UTransformProxy;
class UCombinedTransformGizmo;
class FTransformGizmoDataBinder;

namespace UE::Chaos::ClothAsset
{

class FChaosClothAssetEditorToolkit;
class FChaosClothPreviewScene;
class FClothEditorSimulationVisualization;

/**
 * Viewport client for the 3d sim preview in the cloth editor. Currently same as editor viewport
 * client but doesn't allow editor gizmos/widgets.
 */
class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditor3DViewportClient : public FEditorViewportClient, public TSharedFromThis<FChaosClothAssetEditor3DViewportClient>
{
public:

	FChaosClothAssetEditor3DViewportClient(FEditorModeTools* InModeTools, TSharedPtr<FChaosClothPreviewScene> InPreviewScene, 
		TSharedPtr<FClothEditorSimulationVisualization> InVisualization,
		const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);

	// Call this after construction to initialize callbacks when settings change
	void RegisterDelegates();

	virtual ~FChaosClothAssetEditor3DViewportClient();

	// Delete the viewport gizmo and transform proxy
	void DeleteViewportGizmo();

	void ClearSelectedComponents();

	void EnableSimMeshWireframe(bool bEnable) { bSimMeshWireframe = bEnable; }
	bool SimMeshWireframeEnabled() const { return bSimMeshWireframe; }

	void EnableRenderMeshWireframe(bool bEnable);
	bool RenderMeshWireframeEnabled() const { return bRenderMeshWireframe; }

	void SetClothEdMode(TObjectPtr<UChaosClothAssetEditorMode> ClothEdMode);
	void SetClothEditorToolkit(TWeakPtr<const FChaosClothAssetEditorToolkit> ClothToolkit);

	void SoftResetSimulation();
	void HardResetSimulation();
	void SuspendSimulation();
	void ResumeSimulation();
	bool IsSimulationSuspended() const;
	void SetEnableSimulation(bool bEnable);
	bool IsSimulationEnabled() const;

	// LODIndex == INDEX_NONE is LOD Auto
	void SetLODModel(int32 LODIndex);
	bool IsLODModelSelected(int32 LODIndex) const;
	int32 GetLODModel() const;
	int32 GetNumLODs() const;

	FBox PreviewBoundingBox() const;

	TWeakPtr<FChaosClothPreviewScene> GetClothPreviewScene();
	TWeakPtr<const FChaosClothPreviewScene> GetClothPreviewScene() const;
	UChaosClothComponent* GetPreviewClothComponent();
	const UChaosClothComponent* GetPreviewClothComponent() const;
	TWeakPtr<FClothEditorSimulationVisualization> GetSimulationVisualization() {
		return ClothEditorSimulationVisualization;
	}
	TWeakPtr<const FChaosClothAssetEditorToolkit> GetClothToolKit() const { return ClothToolkit; }

private:

	// FGCObject override
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// FEditorViewportClient overrides
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual void SetWidgetMode(UE::Widget::EWidgetMode NewMode) override {}
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override { return UE::Widget::EWidgetMode::WM_None; }
	virtual void Tick(float DeltaSeconds) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;

	void OnAssetViewerSettingsChanged(const FName& InPropertyName);
	void SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags);

	void ComponentSelectionChanged(UObject* NewSelection);

	TWeakPtr<FChaosClothPreviewScene> ClothPreviewScene;

	TObjectPtr<UChaosClothAssetEditorMode> ClothEdMode;

	TWeakPtr<const FChaosClothAssetEditorToolkit> ClothToolkit;

	TWeakPtr<FClothEditorSimulationVisualization> ClothEditorSimulationVisualization;
	
	bool bSimMeshWireframe = true;
	bool bRenderMeshWireframe = false;

	// Dataflow render support
	Dataflow::FTimestamp LastModifiedTimestamp = Dataflow::FTimestamp::Invalid;

	// Gizmo support
	TObjectPtr<UTransformProxy> TransformProxy = nullptr;
	TObjectPtr<UCombinedTransformGizmo> Gizmo = nullptr;
	TSharedPtr<FTransformGizmoDataBinder> DataBinder = nullptr;

};
} // namespace UE::Chaos::ClothAsset
