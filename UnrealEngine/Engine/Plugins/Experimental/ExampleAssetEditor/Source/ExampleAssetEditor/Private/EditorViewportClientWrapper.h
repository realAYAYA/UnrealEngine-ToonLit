// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

class FEditorModeTools;
class FPreviewScene;
class SEditorViewport;

// Class for overriding where we need to hook in current input router functions
// The flow here is FSceneViewport::InputKey -> FEditorViewportClient::InputKey -> MouseDeltaTracker::StartTracking -> FEditorViewportClient::TrackingStarted
// Only certain viewports (those that have interactive tools contexts set up) currently override this function on the viewport client
class FEditorViewportClientWrapper
	: public FEditorViewportClient
{
public:
	FEditorViewportClientWrapper(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene = nullptr, const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);
};
