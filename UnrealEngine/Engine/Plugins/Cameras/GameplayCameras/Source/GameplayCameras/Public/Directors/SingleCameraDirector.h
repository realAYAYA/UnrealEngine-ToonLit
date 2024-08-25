// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraDirector.h"

#include "SingleCameraDirector.generated.h"

/**
 * A simple camera director that only ever returns one single camera mode.
 */
UCLASS(EditInlineNew)
class USingleCameraDirector : public UCameraDirector
{
	GENERATED_BODY()

public:

	USingleCameraDirector(const FObjectInitializer& ObjectInit);

protected:

	virtual void OnRun(const FCameraDirectorRunParams& Params, FCameraDirectorRunResult& OutResult) override;

public:

	/** The camera mode to run every frame. */
	UPROPERTY(EditAnywhere, Category=Common)
	TObjectPtr<UCameraMode> CameraMode;
};

