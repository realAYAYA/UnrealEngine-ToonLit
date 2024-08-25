// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"

#include "BlendStackCameraNode.generated.h"

class UBlendStackRootCameraNode;
class UCameraAsset;
class UCameraEvaluationContext;
class UCameraMode;
struct FCameraModeTransition;

/**
 * Parameter structure for pushing a camera mode onto a blend stack.
 */
struct FBlendStackCameraPushParams
{
	/** The evaluator currently running.*/
	TObjectPtr<UCameraSystemEvaluator> Evaluator;

	/** The evaluation context within which a camera mode's node tree should run. */
	TWeakObjectPtr<const UCameraEvaluationContext> EvaluationContext;

	/** The source camera mode asset to instantiate and push on the blend stack. */
	TObjectPtr<const UCameraMode> CameraMode;
};

/**
 * A blend stack implemented as a camera node.
 */
UCLASS(MinimalAPI)
class UBlendStackCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:

	/** Push a new camera mode onto the blend stack. */
	void Push(const FBlendStackCameraPushParams& Params);

public:

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	// UCameraNode interface
	virtual FCameraNodeChildrenView OnGetChildren() override;
	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

	const FCameraModeTransition* FindTransition(const FBlendStackCameraPushParams& Params) const;
	const FCameraModeTransition* FindTransition(
			TArrayView<const FCameraModeTransition> Transitions, 
			const UCameraMode* FromCameraMode, const UCameraAsset* FromCameraAsset, bool bFromFrozen,
			const UCameraMode* ToCameraMode, const UCameraAsset* ToCameraAsset) const;

public:

	/** 
	 * Whether to automatically pop camera modes out of the stack when another mode
	 * has reached 100% blend above them.
	 */
	UPROPERTY()
	bool bAutoPop = true;

	/**
	 * Whether to blend-in the first camera mode when the stack is previously empty.
	 */
	UPROPERTY()
	bool bBlendFirstCameraMode = false;

protected:

	struct FCameraModeEntry
	{
		TWeakObjectPtr<const UCameraEvaluationContext> EvaluationContext;
		TObjectPtr<const UCameraMode> OriginalCameraMode;
		TObjectPtr<UBlendStackRootCameraNode> RootNode;
		FCameraNodeRunResult Result;
		bool bIsFirstFrame = false;
		bool bIsFrozen = false;
	};

	TArray<FCameraModeEntry> Entries;
};

