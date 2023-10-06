// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif

class FRDGBuilder;
namespace ERHIFeatureLevel { enum Type : int; }

/** 
 * Interface for a compute task worker.
 * Implementations will queue and schedule work per scene before the renderer submits at fixed points in the frame.
 */
class IComputeTaskWorker
{
public:
	virtual ~IComputeTaskWorker() {}

	/** Returns true if there is any scheduled work. */
	virtual bool HasWork(FName InExecutionGroupName) const = 0;

	/** Add any scheduled work to an RDGBuilder ready for execution. */
	virtual void SubmitWork(FRDGBuilder& GraphBuilder, FName InExecutionGroupName, ERHIFeatureLevel::Type FeatureLevel) = 0;
};

/** Core execution group names for use in IComputeTaskWorker::SubmitWork(). */
struct ComputeTaskExecutionGroup
{
	static ENGINE_API FName Immediate;
	static ENGINE_API FName EndOfFrameUpdate;
};
