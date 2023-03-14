// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PersonaPreviewSceneController.h"
#include "PersonaPreviewSceneDefaultController.generated.h"

UCLASS(DisplayName = "Default")
class UPersonaPreviewSceneDefaultController : public UPersonaPreviewSceneController
{
public:
	GENERATED_BODY()

	virtual void InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const;
	virtual void UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const;
};
