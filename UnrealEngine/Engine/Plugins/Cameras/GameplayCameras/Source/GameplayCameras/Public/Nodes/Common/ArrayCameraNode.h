// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Core/CameraNode.h"

#include "ArrayCameraNode.generated.h"

/**
 * A camera node that runs a list of other camera nodes.
 */
UCLASS(MinimalAPI)
class UArrayCameraNode : public UCameraNode
{
	GENERATED_BODY()

protected:

	virtual FCameraNodeChildrenView OnGetChildren() override;
	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

public:

	/** The camera nodes to run. */
	UPROPERTY(EditAnywhere, Category=Common)
	TArray<TObjectPtr<UCameraNode>> Children;
};

