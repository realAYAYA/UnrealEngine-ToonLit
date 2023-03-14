// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/FMutableViewportClient.h"

#include "AdvancedPreviewScene.h"
#include "Editor.h"
#include "UnrealWidget.h"

FMutableMeshViewportClient::FMutableMeshViewportClient(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene)
: FEditorViewportClient(&GLevelEditorModeTools(), &InPreviewScene.Get())
{
	// Remove the initial gizmo
	Widget->SetDefaultVisibility(false);
	
	FAdvancedPreviewScene* PreviewSceneCasted = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	//PreviewSceneCasted->SetEnvironmentVisibility(false);
	PreviewSceneCasted->SetFloorVisibility(false);
}
