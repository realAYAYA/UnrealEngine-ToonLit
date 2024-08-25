// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraInstantiableObject.h"
#include "Core/CameraPose.h"
#include "Core/CameraNodeChildrenView.h"
#include "CoreTypes.h"
#include "UObject/Object.h"

#include "CameraNode.generated.h"

class UCameraSystemEvaluator;

/**
 * Parameter structure for running a camera node.
 */
struct FCameraNodeRunParams
{
	/** The evaluation running this evaluation.*/
	TObjectPtr<UCameraSystemEvaluator> Evaluator;
	/** The time interval for the evaluation. */
	float DeltaTime = 0.f;
	/** Whether this is the first evaluation of this camera node hierarchy. */
	bool bIsFirstFrame = false;
};

/**
 * Input/output result structure for running a camera node.
 */
struct FCameraNodeRunResult
{
	/** The camera pose. */
	FCameraPose CameraPose;
	/** Whether the current frame is a camera cut. */
	bool bIsCameraCut = false;
	/** Whether this result is valid. */
	bool bIsValid = false;

	/** Reset this result to its default (non-valid) state. */
	void Reset();
};

UENUM()
enum class ECameraNodeFlags
{
	None = 0,
	RequiresReset = 1
};
ENUM_CLASS_FLAGS(ECameraNodeFlags);

struct FCameraNodeResetParams
{
};

/**
 * The base class for a camera node.
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, MinimalAPI)
class UCameraNode : public UCameraInstantiableObject
{
	GENERATED_BODY()

public:

	/** Get the list of children under this node. */
	FCameraNodeChildrenView GetChildren();

	/** Get the flags for this node. */
	ECameraNodeFlags GetNodeFlags() const { return Flags; }

	/** Resets this node. */
	void Reset(const FCameraNodeResetParams& Params);

	/** Run this node. */
	void Run(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult);

protected:

	/** Sets the flags for this node. Should only be called once during construction. */
	void SetNodeFlags(ECameraNodeFlags InFlags) { Flags = InFlags; }

	/** Get the list of children under this node. */
	virtual FCameraNodeChildrenView OnGetChildren() { return FCameraNodeChildrenView(); }

	/** Resets this node. */
	virtual void OnReset(const FCameraNodeResetParams& Params) {}

	/** Run this node. */
	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) {}

#if WITH_EDITOR

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

public:

	/** Specifies whether this node is enabled. */
	UPROPERTY(EditAnywhere, Category=Common)
	bool bIsEnabled = true;

private:

	/** The flags for this node. Should only be set once during construction. */
	ECameraNodeFlags Flags = ECameraNodeFlags::None;
};

