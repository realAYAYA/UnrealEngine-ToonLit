// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPersonaEditMode.h"

class FPhysicsControlProfileEditorToolkit;

/**
 * The Physics Control Profile Persona editor mode.
 */
class PHYSICSCONTROLEDITOR_API FPhysicsControlProfileEditorMode	: public IPersonaEditMode
{
public:
	/** The name of the mode. */
	static FName ModeName;

	void SetEditorToolkit(FPhysicsControlProfileEditorToolkit* InToolkit) { PhysicsControlProfileEditorToolkit = InToolkit; }

	// IPersonaEditMode overrides.
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override {}
	// ~END IPersonaEditMode overrides.

	// FEdMode overrides.
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
	virtual bool AllowWidgetMove() override { return false; }
	virtual bool ShouldDrawWidget() const override { return false; }
	virtual bool UsesTransformWidget() const override { return false; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return false; }
	// ~END FEdMode overrides.

protected:
	FPhysicsControlProfileEditorToolkit* PhysicsControlProfileEditorToolkit = nullptr;
};
