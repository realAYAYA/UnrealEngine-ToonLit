// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryBase.h" // Predeclare macros

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "InteractionMechanic.h"
#include "InteractiveTool.h"
#include "Selection/UVToolSelection.h"
#include "Selection/UVToolSelectionAPI.h" // EUVEditorSelectionMode
#include "Mechanics/RectangleMarqueeMechanic.h"
#include "ToolContextInterfaces.h" //FViewCameraState

#include "UVEditorMeshSelectionMechanic.generated.h"

class APreviewGeometryActor;
struct FCameraRectangle;
class ULineSetComponent;
class UMaterialInstanceDynamic;
class UPointSetComponent;
class UTriangleSetComponent;
class UUVToolViewportButtonsAPI;
class ULocalSingleClickInputBehavior;
class ULocalMouseHoverBehavior;

/**
 * Mechanic for selecting elements of a dynamic mesh in the UV editor. Interacts
 * heavily with UUVToolSelectionAPI, which actually stores selections.
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorMeshSelectionMechanic : public UInteractionMechanic
{
	GENERATED_BODY()

public:
	using FDynamicMeshAABBTree3 = UE::Geometry::FDynamicMeshAABBTree3;
	using FUVToolSelection = UE::Geometry::FUVToolSelection;

	virtual ~UUVEditorMeshSelectionMechanic() {}

	virtual void Setup(UInteractiveTool* ParentTool) override;
	virtual void Shutdown() override;

	// Initialization functions.
	// The selection API is provided as a parameter rather than being grabbed out of the context
	// store mainly because UVToolSelectionAPI itself sets up a selection mechanic, and is not 
	// yet in the context store when it does this. 
	void Initialize(UWorld* World, UWorld* LivePreviewWorld, UUVToolSelectionAPI* SelectionAPI);
	void SetTargets(const TArray<TObjectPtr<UUVEditorToolMeshInput>>& TargetsIn);

	void SetIsEnabled(bool bIsEnabled);
	bool IsEnabled() { return bIsEnabled; };

	void SetShowHoveredElements(bool bShow);

	using ESelectionMode = UUVToolSelectionAPI::EUVEditorSelectionMode;
	using FModeChangeOptions = UUVToolSelectionAPI::FSelectionMechanicModeChangeOptions;
	/**
	 * Sets selection mode for the mechanic.
	 */
	void SetSelectionMode(ESelectionMode TargetMode,
		const FModeChangeOptions& Options = FModeChangeOptions());
	
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI);
	virtual void LivePreviewRender(IToolsContextRenderAPI* RenderAPI);
	virtual void LivePreviewDrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI);
	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos, bool bSourceIsLivePreview);
	virtual void OnClicked(const FInputDeviceRay& ClickPos, bool bSourceIsLivePreview);

	// IModifierToggleBehaviorTarget implementation
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn);

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos, bool bSourceIsLivePreview);
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos);
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos, bool bSourceIsLivePreview);
	virtual void OnEndHover();

	/**
	 * Broadcasted whenever the marquee mechanic rectangle is changed, since these changes
	 * don't trigger normal selection broadcasts.
	 */ 
	FSimpleMulticastDelegate OnDragSelectionChanged;

protected:

	UPROPERTY()
	TObjectPtr<UUVToolSelectionAPI> SelectionAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolViewportButtonsAPI> ViewportButtonsAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolEmitChangeAPI> EmitChangeAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolLivePreviewAPI> LivePreviewAPI = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalSingleClickInputBehavior> UnwrapClickTargetRouter = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalSingleClickInputBehavior> LivePreviewClickTargetRouter = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalMouseHoverBehavior> UnwrapHoverBehaviorTargetRouter = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalMouseHoverBehavior> LivePreviewHoverBehaviorTargetRouter = nullptr;

	UPROPERTY()
	TObjectPtr<URectangleMarqueeMechanic> MarqueeMechanic = nullptr;
	
	UPROPERTY()
	TObjectPtr<URectangleMarqueeMechanic> LivePreviewMarqueeMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HoverTriangleSetMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<APreviewGeometryActor> HoverGeometryActor = nullptr;
	// Weak pointers so that they go away when geometry actor is destroyed
	TWeakObjectPtr<UTriangleSetComponent> HoverTriangleSet = nullptr;
	TWeakObjectPtr<ULineSetComponent> HoverLineSet = nullptr;
	TWeakObjectPtr<UPointSetComponent> HoverPointSet = nullptr;

	UPROPERTY()
	TObjectPtr<UInputBehaviorSet> LivePreviewBehaviorSet = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalInputBehaviorSource> LivePreviewBehaviorSource = nullptr;

	TWeakObjectPtr<UInputRouter> LivePreviewInputRouter = nullptr;

	UPROPERTY()
	TObjectPtr<APreviewGeometryActor> LivePreviewHoverGeometryActor = nullptr;
	// Weak pointers so that they go away when geometry actor is destroyed
	TWeakObjectPtr<UTriangleSetComponent> LivePreviewHoverTriangleSet = nullptr;
	TWeakObjectPtr<ULineSetComponent> LivePreviewHoverLineSet = nullptr;
	TWeakObjectPtr<UPointSetComponent> LivePreviewHoverPointSet = nullptr;

	// Should be the same as the mode-level targets array, indexed by AssetID
	TSharedPtr<FDynamicMeshAABBTree3> GetMeshSpatial(int32 TargetId, bool bUseUnwrap);
	TArray<TObjectPtr<UUVEditorToolMeshInput>> Targets;
	TArray<TSharedPtr<FDynamicMeshAABBTree3>> UnwrapMeshSpatials; // 1:1 with Targets
	TArray<TSharedPtr<FDynamicMeshAABBTree3>> AppliedMeshSpatials; // 1:1 with Targets

	ESelectionMode SelectionMode;
	bool bIsEnabled = false;
	bool bShowHoveredElements = true;

	bool GetHitTid(const FInputDeviceRay& ClickPos, int32& TidOut,
		int32& AssetIDOut, bool bUseUnwrap, int32* ExistingSelectionObjectIndexOut = nullptr);
	void ModifyExistingSelection(TSet<int32>& SelectionSetToModify, const TArray<int32>& SelectedIDs);

	FViewCameraState CameraState;

	bool bShiftToggle = false;
	bool bCtrlToggle = false;
	static const int32 ShiftModifierID = 1;
	static const int32 CtrlModifierID = 2;

	// All four combinations of shift/ctrl down are assigned a behaviour
	bool ShouldAddToSelection() const { return !bCtrlToggle && bShiftToggle; }
	bool ShouldRemoveFromSelection() const { return bCtrlToggle && !bShiftToggle; }
	bool ShouldToggleFromSelection() const { return bCtrlToggle && bShiftToggle; }
	bool ShouldRestartSelection() const { return !bCtrlToggle && !bShiftToggle; }

	// For marquee mechanic
	void OnDragRectangleStarted();
	void OnDragRectangleChanged(const FCameraRectangle& CurrentRectangle, bool bSourceIsLivePreview);
	void OnDragRectangleFinished(const FCameraRectangle& Rectangle, bool bCancelled, bool bSourceIsLivePreview);

	TArray<FUVToolSelection> PreDragSelections;
	TArray<FUVToolSelection> PreDragUnsetSelections;
	// Maps asset id to a pre drag selection so that it is easy to tell which assets
	// started with a selection. 1:1 with Targets.
	TArray<const FUVToolSelection*> AssetIDToPreDragSelection;
	TArray<const FUVToolSelection*> AssetIDToPreDragUnsetSelection;
};

