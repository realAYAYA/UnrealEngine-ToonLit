// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"

#include "OffsetCameraNode.generated.h"

/**
 * A camera node that offsets the location of the camera.
 */
UCLASS(MinimalAPI)
class UOffsetCameraNode : public UCameraNode
{
	GENERATED_BODY()

protected:

	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

public:

	/** The offset to apply to the camera, in local space. */
	UPROPERTY(EditAnywhere, Category=Common)
	FVector3d Offset;
};

