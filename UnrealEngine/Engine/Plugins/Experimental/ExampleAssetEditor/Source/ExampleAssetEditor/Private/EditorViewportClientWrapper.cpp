// Copyright Epic Games, Inc.All Rights Reserved.

#include "EditorViewportClientWrapper.h"
#include "UnrealWidget.h"

FEditorViewportClientWrapper::FEditorViewportClientWrapper(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	Widget->SetUsesEditorModeTools(ModeTools.Get());
}
