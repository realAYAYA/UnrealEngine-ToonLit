// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/BlendCameraNode.h"

#include "SimpleBlendCameraNode.generated.h"

/**
 * Result structure for defining a simple scalar-factor-based blend.
 */
struct FSimpleBlendCameraNodeRunResult
{
	float BlendFactor = 0.f;
};

/**
 * Base class for a blend camera node that uses a simple scalar factor.
 */
UCLASS(MinimalAPI, Abstract)
class USimpleBlendCameraNode : public UBlendCameraNode
{
	GENERATED_BODY()

public:

	/** Gets the last evaluated blend factor. */
	float GetBlendFactor() const { return BlendFactor; }

protected:

	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;
	virtual void OnBlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult) override;

	virtual void OnComputeBlendFactor(const FCameraNodeRunParams& Params, FSimpleBlendCameraNodeRunResult& OutResult) {}

	void SetBlendFinished() { bIsBlendFinished = true; }

private:

	float BlendFactor = 0.f;
	bool bIsBlendFinished = false;
};

/**
 * Base class for a blend camera node that uses a simple scalar factor over a fixed time.
 */
UCLASS(MinimalAPI, Abstract)
class USimpleFixedTimeBlendCameraNode : public USimpleBlendCameraNode
{
	GENERATED_BODY()

public:

	/** Duration of the blend. */
	UPROPERTY(EditAnywhere, Category=Blending)
	float BlendTime = 1.f;

protected:

	virtual void OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult) override;

	float GetTimeFactor() const { return CurrentTime / BlendTime; }

private:

	float CurrentTime = 0.f;
};

