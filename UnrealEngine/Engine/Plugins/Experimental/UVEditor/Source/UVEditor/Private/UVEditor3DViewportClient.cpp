// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditor3DViewportClient.h"

#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseWheelBehavior.h"
#include "ContextObjectStore.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "Drawing/MeshDebugDrawing.h"
#include "FrameTypes.h"
#include "UVEditorMode.h"
#include "ContextObjects/UVToolViewportButtonsAPI.h"

FUVEditor3DViewportClient::FUVEditor3DViewportClient(FEditorModeTools* InModeTools,
	FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget,
	UUVToolViewportButtonsAPI* ViewportButtonsAPIIn)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget), 
	ViewportButtonsAPI(ViewportButtonsAPIIn)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);
}

void FUVEditor3DViewportClient::FocusCameraOnSelection()
{
	if (ViewportButtonsAPI) 
	{
		ViewportButtonsAPI->InitiateFocusCameraOnSelection();
	}
}