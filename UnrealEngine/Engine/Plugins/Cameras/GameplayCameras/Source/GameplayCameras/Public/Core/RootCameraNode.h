// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"

#include "RootCameraNode.generated.h"

class UCameraEvaluationContext;
class UCameraMode;

/**
 * Defines evaluation layers for camera modes.
 */
UENUM()
enum class ECameraModeLayer
{
	Base,
	Main,
	Global,
	Visual,
	User0,
	User1,
	User2
};
ENUM_CLASS_FLAGS(ECameraModeLayer)

/**
 * Parameter structure for activating a new camera mode.
 */
struct FActivateCameraModeParams
{
	/** The evaluator currently running.*/
	TObjectPtr<UCameraSystemEvaluator> Evaluator;

	/** The evaluation context in which the camera mode runs. */
	TWeakObjectPtr<const UCameraEvaluationContext> EvaluationContext;

	/** The source camera mode asset that will be instantiated. */
	TObjectPtr<const UCameraMode> CameraMode;

	/** The evaluation layer on which to instantiate the camera mode. */
	ECameraModeLayer Layer = ECameraModeLayer::Main;
};

/**
 * The base class for a camera node that can act as the root of the
 * camera system evaluation.
 */
UCLASS(MinimalAPI, Abstract)
class URootCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:

	/** Activates a camera mode. */
	void ActivateCameraMode(const FActivateCameraModeParams& Params);

private:

	/** Activates a camera mode. */
	virtual void OnActivateCameraMode(const FActivateCameraModeParams& Params) {}
};

