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
class FComputeKernelShader;

/** 
 * Class that manages the scheduling of Compute Graph work.
 * Work can be enqueued on the render thread for the execution at the next call to ExecuteBatches().
 */
class FComputeGraphTaskWorker : public IComputeTaskWorker
{
public:
	/** Enqueue a compute graph for execution. */
	void Enqueue(
		FName InExecutionGroupName,
		FName InOwnerName,
		uint8 InGraphSortPriority,
		FComputeGraphRenderProxy const* InGraphRenderProxy, 
		TArray<FComputeDataProviderRenderProxy*> InDataProviderRenderProxies,
		FSimpleDelegate InFallbackDelegate,
		const UObject* OwnerPointer = nullptr);

	void Abort(const UObject* OwnerPointer);

	/** Has enqueued compute graph work. */
	bool HasWork(FName InExecutionGroupName) const override;

	/** Submit enqueued compute graph work. */
	void SubmitWork(
		FRDGBuilder& GraphBuilder,
		FName InExecutionGroupName,
		ERHIFeatureLevel::Type InFeatureLevel ) override;

private:
	/** Description of each graph that is enqueued. */
	struct FGraphInvocation
	{
		/** Name of owner object that invoked the graph. */
		FName OwnerName;
		/** Pointer to an owning UObject. */
		const UObject* OwnerPointer = nullptr;
		/** Priority used when sorting work. */
		uint8 GraphSortPriority = 0;
		/** Graph render proxy. */
		FComputeGraphRenderProxy const* GraphRenderProxy = nullptr;
		/** Data provider render proxies. */
		TArray<FComputeDataProviderRenderProxy*> DataProviderRenderProxies;
		/** Render thread fallback logic for invocations that are invalid. */
		FSimpleDelegate FallbackDelegate;
		
		~FGraphInvocation();
	};

	/** Map of enqueued work per execution group . */
	TMap<FName, TArray<FGraphInvocation> > GraphInvocationsPerGroup;

	/** Description of a single dispatch group to be submitted. */
	struct FSubmitDescription
	{
		/**
		 * Sort key allows us to sort dispatches for optimum scheduling.
		 * Syncing is usually required between consecutive kernels in a graph.
		 * So we schedule the first kernels of all the graphs, before all of the second kernels.
		 * That reduces time sync time, at the expense of memory pressure for buffers that need to stay alive.
		 * In futuer we may want to add a limit to the number of graphs in flight to avoid memory pressure.
		 */
		union
		{
			uint32 PackedSortKey = 0;
			struct
			{
				uint32 GraphIndex : 12;			// Graph index.
				uint32 KernelIndex : 12;		// Kernel index within the graph.
				uint32 GraphSortPriority : 8;	// Externally defined sort priority to maintain inter-graph dependencies.
			};
		};

		/** Track the index into our collected shader array. */
		uint32 ShaderIndex : 15;
		/** Track if this is Unified Dispatch. */
		uint32 bIsUnified : 1;
	};

	// These arrays could be local to FComputeGraphTaskWorker::SubmitWork() but we 
	// store them with class and Reset() them at each usage to avoid per frame array allocations.
	TArray<FSubmitDescription> SubmitDescs;
	TArray<TShaderRef<FComputeKernelShader>> Shaders;
	TArray<int32> PermutationIds;
	TArray<FIntVector> ThreadCounts;
};
