// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"
#include "Core/CameraNodeTreeCache.h"

#include "BlendStackRootCameraNode.generated.h"

class UBlendCameraNode;
class UCameraMode;

/**
 * Root camera node for running a camera mode in a blend stack.
 * This camera node wraps both the camera mode's root node, and the
 * blend node used to blend it.
 */
UCLASS(MinimalAPI)
class UBlendStackRootCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:

	/** Initialize this node. */
	void Initialize(UBlendCameraNode* InBlend, UCameraNode* InRootNode);

	/** Gets the blend node. */
	UBlendCameraNode* GetBlend() const { return Blend; }

	/** Gets the root of the camera mode. */
	UCameraNode* GetRootNode() const { return RootNode; }

protected:

	virtual FCameraNodeChildrenView OnGetChildren() override;
	virtual void OnReset(const FCameraNodeResetParams& Params) override;
	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

private:

	/** The blend to use on the camera mode. */
	UPROPERTY()
	TObjectPtr<UBlendCameraNode> Blend;

	/** The root of the instantied camera node tree. */
	UPROPERTY()
	TObjectPtr<UCameraNode> RootNode;

	/** Node cache for the hierarchy below the root node. */
	FCameraNodeTreeCache TreeCache;
};

