// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "Tools/Modes.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SharedPointer.h"
#include "Tools/LegacyEdModeInterfaces.h"

#include "LegacyEdMode.generated.h"

class AActor;
class FEditorModeTools;
class FEdMode;
class UTexture2D;
class FEditorViewportClient;
class FViewport;
class UPrimitiveComponent;

UCLASS()
class UNREALED_API ULegacyEdModeWrapper final : public UEdMode, public ILegacyEdModeWidgetInterface, public ILegacyEdModeToolInterface, public ILegacyEdModeSelectInterface, public ILegacyEdModeDrawHelperInterface, public ILegacyEdModeViewportInterface
{
	GENERATED_BODY()

public:
	ULegacyEdModeWrapper();

	bool CreateLegacyMode(FEditorModeID ModeID, FEditorModeTools& ModeManager);

	// Begin UEdMode overrides
	virtual void Initialize() override;
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelected) const override;
	virtual bool Select(AActor* InActor, bool bInSelected) override;
	virtual bool ProcessEditDuplicate() override;
	virtual bool ProcessEditDelete() override;
	virtual bool ProcessEditCut() override;
	virtual bool ProcessEditCopy() override;
	virtual bool ProcessEditPaste() override;
	virtual EEditAction::Type GetActionEditDuplicate() override;
	virtual EEditAction::Type GetActionEditDelete() override;
	virtual EEditAction::Type GetActionEditCut() override;
	virtual EEditAction::Type GetActionEditCopy() override;
	virtual EEditAction::Type GetActionEditPaste() override;
	virtual bool IsSnapRotationEnabled() override;
	virtual bool SnapRotatorToGridOverride(FRotator& Rotation) override;
	virtual void ActorsDuplicatedNotify(TArray<AActor*>& PreDuplicateSelection, TArray<AActor*>& PostDuplicateSelection, bool bOffsetLocations) override;
	virtual void ActorMoveNotify() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void ActorPropChangeNotify() override;
	virtual void UpdateInternalData() override;
	virtual void MapChangeNotify() override;
	virtual void SelectNone() override;
	virtual bool GetPivotForOrbit(FVector& OutPivot) const override;
	virtual void PostUndo() override;
	virtual bool DisallowMouseDeltaTracking() const override;
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override;
	virtual bool GetOverrideCursorVisibility(bool& bWantsOverride, bool& bHardwareCursorVisible, bool bSoftwareCursorVisible) const;
	virtual bool CanAutoSave() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool ComputeBoundingBoxForViewportFocus(AActor* Actor, UPrimitiveComponent* PrimitiveComponent, FBox& InOutBox) const override;
	virtual bool AllowsViewportDragTool() const override;
	virtual bool UsesToolkits() const override;

	virtual bool ProcessCapturedMouseMoves(FEditorViewportClient* InViewportClient, FViewport* InViewport, const TArrayView<FIntPoint>& CapturedMouseMoves) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool InputAxis(FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime) override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;
	virtual bool MouseEnter(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool ReceivedFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool LostFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool PreConvertMouseMovement(FEditorViewportClient* InViewportClient) override;
	virtual bool PostConvertMouseMovement(FEditorViewportClient* InViewportClient) override;

	virtual bool ShouldDrawBrushWireframe(AActor* InActor) const override;
	virtual void Enter() override;
	virtual void Exit() override;

	virtual FEdMode* AsLegacyMode() override;
	virtual UTexture2D* GetVertexTexture() override;
	// End UEdMode overrides

	// ILegacyEdModeWidgetInterface
	virtual bool AllowWidgetMove() override;
	virtual bool CanCycleWidgetMode() const override;
	virtual bool ShowModeWidgets() const override;
	virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;
	virtual FVector GetWidgetNormalFromCurrentAxis(void* InData) override;
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true) override;
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true) override;
	virtual void SetCurrentWidgetAxis(EAxisList::Type InAxis) override;
	virtual EAxisList::Type GetCurrentWidgetAxis() const override;
	virtual bool UsesPropertyWidgets() const override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	// End ILegacyEdModeWidgetInterface

	// ILegacyEdModeToolInterface overrides
	virtual void SetCurrentTool(EModeTools InID) override;
	virtual void SetCurrentTool(FModeTool* InModeTool) override;
	virtual FModeTool* FindTool(EModeTools InID) override;
	virtual const TArray<FModeTool*>& GetTools() const override;
	virtual FModeTool* GetCurrentTool() override;
	virtual const FModeTool* GetCurrentTool() const override;
	// End ILegacyEdModeToolInterface overrides

	// Start FCommonDrawHelper overrides
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	// End FCommonDrawHelper overrides

private:
	TSharedPtr<FEdMode> LegacyEditorMode;
};