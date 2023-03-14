// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeWorkerInterface.h"
#include "RHIDefinitions.h"
#include "Shader.h"

class FComputeDataProviderRenderProxy;
class FComputeGraphRenderProxy;
class FComputeKernelResource;

/** 
 * Class that manages the scheduling of Compute Graph work.
 * Work can be enqueued on the render thread for the execution at the next call to ExecuteBatches().
 */
class FComputeGraphTaskWorker : public IComputeTaskWorker
{
public:
	/** Enqueue a compute graph for execution. */
	void Enqueue(
		FName InOwnerName,
		FComputeGraphRenderProxy const* InGraphRenderProxy, 
		TArray<FComputeDataProviderRenderProxy*> InDataProviderRenderProxies );

	/** Submit enqueued compute graph work. */
	void SubmitWork(
		FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel ) override;

private:
	/** Description of each graph that is enqueued. */
	struct FGraphInvocation
	{
		/** Name of owner object that invoked the graph. */
		FName OwnerName;
		/** Graph render proxy. */
		FComputeGraphRenderProxy const* GraphRenderProxy = nullptr;
		/** Data provider render proxies. */
		TArray<FComputeDataProviderRenderProxy*> DataProviderRenderProxies;
		
		~FGraphInvocation();
	};

	TArray<FGraphInvocation> GraphInvocations;
};
