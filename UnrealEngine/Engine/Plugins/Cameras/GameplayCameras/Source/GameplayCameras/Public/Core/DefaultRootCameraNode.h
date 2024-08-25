// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/RootCameraNode.h"

#include "DefaultRootCameraNode.generated.h"

class UBlendStackCameraNode;

/**
 * The default implementation of a root camera node.
 */
UCLASS(MinimalAPI)
class UDefaultRootCameraNode : public URootCameraNode
{
	GENERATED_BODY()

public:

	UDefaultRootCameraNode(const FObjectInitializer& ObjectInit);

protected:

	virtual FCameraNodeChildrenView OnGetChildren() override;
	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

	virtual void OnActivateCameraMode(const FActivateCameraModeParams& Params) override;

private:

	UPROPERTY(Instanced)
	TObjectPtr<UCameraNode> BaseLayer;

	UPROPERTY(Instanced)
	TObjectPtr<UCameraNode> MainLayer;

	UPROPERTY(Instanced)
	TObjectPtr<UCameraNode> GlobalLayer;

	UPROPERTY(Instanced)
	TObjectPtr<UCameraNode> VisualLayer;
};

