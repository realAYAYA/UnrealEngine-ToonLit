// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IKRetargetEditorController.h"

#include "Retargeter/IKRetargeter.h"
#include "IPersonaEditMode.h"

class UIKRigProcessor;
class FIKRetargetEditorController;
class FIKRetargetEditor;
class FIKRetargetPreviewScene;


class FIKRetargetDefaultMode : public IPersonaEditMode
{
public:
	static FName ModeName;
	
	FIKRetargetDefaultMode() = default;

	/** glue for all the editor parts to communicate */
	void SetEditorController(const TSharedPtr<FIKRetargetEditorController> InEditorController) { EditorController = InEditorController; };

	/** IPersonaEditMode interface */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;
	/** END IPersonaEditMode interface */

	/** FEdMode interface */
	virtual void Initialize() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
	virtual bool AllowWidgetMove() override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override;

	virtual void Enter() override;
	virtual void Exit() override;
	// IS THIS NEEDED
	virtual bool IsSelectionAllowed( AActor* InActor, bool bInSelection ) const override { return true; }
	/** END FEdMode interface */

private:
	void RenderDebugProxies(FPrimitiveDrawInterface* PDI, const FIKRetargetEditorController* Controller) const;
	static void ApplyOffsetToMeshTransform(const FVector& Offset, USceneComponent* Component);

	// the skeleton currently being edited
	UDebugSkelMeshComponent* GetCurrentlyEditedMesh() const;
	ERetargetSourceOrTarget SkeletonMode;
	
	/** The hosting app */
	TWeakPtr<FIKRetargetEditorController> EditorController;

	UE::Widget::EWidgetMode CurrentWidgetMode;
	bool bIsTranslating = false;

	bool bIsInitialized = false;
};
