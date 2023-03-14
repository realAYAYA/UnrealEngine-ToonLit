// Copyright Epic Games, Inc.All Rights Reserved.

#include "LevelAssetEditorViewportClient.h"
#include "UnrealWidget.h"

FLevelAssetEditorViewportClient::FLevelAssetEditorViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	Widget->SetUsesEditorModeTools(ModeTools.Get());
}
