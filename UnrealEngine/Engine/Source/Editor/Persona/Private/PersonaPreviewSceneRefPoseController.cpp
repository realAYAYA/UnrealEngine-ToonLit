// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaPreviewSceneRefPoseController.h"
#include "AnimationEditorPreviewScene.h"

void UPersonaPreviewSceneRefPoseController::InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{
	PreviewScene->ShowReferencePose(true, bResetBoneTransforms);
}

void UPersonaPreviewSceneRefPoseController::UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{

}