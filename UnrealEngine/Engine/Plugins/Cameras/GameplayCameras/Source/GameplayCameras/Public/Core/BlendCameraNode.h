// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"

#include "BlendCameraNode.generated.h"

/**
 * Parameter struct for blending camera node tree results.
 */
struct FCameraNodeBlendParams
{
	FCameraNodeBlendParams(
			const FCameraNodeRunParams& InChildParams,
			const FCameraNodeRunResult& InChildResult)
		: ChildParams(InChildParams)
		, ChildResult(InChildResult)
	{}

	/** The parameters that the blend received during the evaluation. */
	const FCameraNodeRunParams& ChildParams;
	/** The result that the blend should apply over another result. */
	const FCameraNodeRunResult& ChildResult;
};

/**
 * Result struct for blending camera node tree results.
 */
struct FCameraNodeBlendResult
{
	FCameraNodeBlendResult(FCameraNodeRunResult& InBlendedResult)
		: BlendedResult(InBlendedResult)
	{}

	/** The result upon which another result should be blended. */
	FCameraNodeRunResult& BlendedResult;

	/** Whether the blend has reached 100%. */
	bool bIsBlendFull = false;

	/** Whether the blend is finished. */
	bool bIsBlendFinished = false;
};

/**
 * Base class for blend camera nodes.
 */
UCLASS(MinimalAPI)
class UBlendCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:

	/** Blend the result of a camera node tree over another result. */
	void BlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult);

protected:

	/** Blend the result of a camera node tree over another result. */
	virtual void OnBlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult) {}
};

