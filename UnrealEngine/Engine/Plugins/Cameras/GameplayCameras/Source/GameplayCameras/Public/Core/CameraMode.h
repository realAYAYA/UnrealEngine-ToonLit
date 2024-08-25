// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraModeTransition.h"
#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"

#include "CameraMode.generated.h"

class UCameraNode;

/**
 * A camera mode asset, which runs a hierarchy of camera nodes to drive 
 * the behavior of a camera.
 */
UCLASS(MinimalAPI)
class UCameraMode : public UObject
{
	GENERATED_BODY()

public:

	/** Root camera node. */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TObjectPtr<UCameraNode> RootNode;

	/** List of enter transitions for this camera mode. */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> EnterTransitions;

	/** List of exist transitions for this camera mode. */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> ExitTransitions;
};

