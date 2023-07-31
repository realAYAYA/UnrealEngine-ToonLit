// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AnimationEditorPreviewActor.generated.h"

UCLASS()
class PERSONA_API AAnimationEditorPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	/** AActor interface */
	virtual void K2_DestroyActor() override;
};
