// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"

#include "CameraDirector.generated.h"

class UCameraEvaluationContext;
class UCameraMode;

/**
 * Parameter structure for running a camera director.
 */
struct FCameraDirectorRunParams
{
	/** Time interval for the update. */
	float DeltaTime = 0.f;

	/** The context in which this director runs. */
	const UCameraEvaluationContext* OwnerContext = nullptr;
};

/**
 * Result struct for running a camera director.
 */
struct FCameraDirectorRunResult
{
	/** The camera mode(s) that the director says should be active this frame. */
	TArray<TObjectPtr<const UCameraMode>, TInlineAllocator<2>> ActiveCameraModes;
};

/**
 * Base class for a camera director.
 */
UCLASS(Abstract, DefaultToInstanced, MinimalAPI)
class UCameraDirector : public UObject
{
	GENERATED_BODY()

public:
	
	/** Runs the camera director to determine what camera mode(s) should be active this frame. */
	void Run(const FCameraDirectorRunParams& Params, FCameraDirectorRunResult& OutResult);

protected:

	/** Runs the camera director to determine what camera mode(s) should be active this frame. */
	virtual void OnRun(const FCameraDirectorRunParams& Params, FCameraDirectorRunResult& OutResult) {}
};

