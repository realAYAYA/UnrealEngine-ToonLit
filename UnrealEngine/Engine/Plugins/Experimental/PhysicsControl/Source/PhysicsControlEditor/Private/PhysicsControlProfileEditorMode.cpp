// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileEditorMode.h"
#include "PhysicsControlEditorModule.h"
#include "PhysicsControlProfileEditorToolkit.h"
#include "AssetEditorModeManager.h"

FName FPhysicsControlProfileEditorMode::ModeName("PhysicsControlProfileAssetEditMode");

//======================================================================================================================
bool FPhysicsControlProfileEditorMode::GetCameraTarget(FSphere& OutTarget) const
{
	return false;
}

//======================================================================================================================
IPersonaPreviewScene& FPhysicsControlProfileEditorMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

//======================================================================================================================
void FPhysicsControlProfileEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
}

//======================================================================================================================
void FPhysicsControlProfileEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
}

//======================================================================================================================
void FPhysicsControlProfileEditorMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
}
