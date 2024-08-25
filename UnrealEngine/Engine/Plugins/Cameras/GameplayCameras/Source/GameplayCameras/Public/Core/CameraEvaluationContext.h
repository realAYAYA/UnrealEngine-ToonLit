// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"
#include "Core/CameraNode.h"

#include "CameraEvaluationContext.generated.h"

class UCameraAsset;

/**
 * Base class for providing a context to running camera modes.
 */
UCLASS(MinimalAPI)
class UCameraEvaluationContext : public UObject
{
	GENERATED_BODY()

public:

	UCameraEvaluationContext(const FObjectInitializer& ObjectInit);

	/** Gets the camera asset that is hosted in this context. */
	UCameraAsset* GetCameraAsset() const { return CameraAsset; }

	/** Gets the initial evaluation result for all camera modes in this context. */
	const FCameraNodeRunResult& GetInitialResult() const { return InitialResult; }

protected:

	/** The camera asset hosted in this context. */
	UPROPERTY()
	TObjectPtr<UCameraAsset> CameraAsset;

	/** The initial result for all camera modes in this context. */
	FCameraNodeRunResult InitialResult;
};

