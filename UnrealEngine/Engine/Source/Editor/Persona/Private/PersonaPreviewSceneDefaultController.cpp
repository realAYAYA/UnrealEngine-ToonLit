// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaPreviewSceneDefaultController.h"
#include "AnimationEditorPreviewScene.h"

void UPersonaPreviewSceneDefaultController::InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{
	PreviewScene->ShowDefaultMode();
}

void UPersonaPreviewSceneDefaultController::UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{

}