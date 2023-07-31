// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

/** 
 * Interface for a compute task worker.
 * Implementations will queue and schedule work per scene before the renderer submits at fixed points in the frame.
 */
class ENGINE_API IComputeTaskWorker
{
public:
	virtual ~IComputeTaskWorker() {}

	/** Add any scheduled work to an RDGBuilder ready for execution. */
	virtual void SubmitWork(class FRHICommandListImmediate& RHIComdList, ERHIFeatureLevel::Type FeatureLevel) = 0;
};
