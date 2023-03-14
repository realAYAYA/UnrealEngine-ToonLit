// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeGraphWorker.h"

#include "ComputeFramework/ComputeKernel.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ComputeKernelShader.h"
#include "ComputeFramework/ComputeGraph.h"
#include "ComputeFramework/ComputeGraphRenderProxy.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIDefinitions.h"

DECLARE_GPU_STAT_NAMED(ComputeFramework_ExecuteBatches, TEXT("ComputeFramework::ExecuteBatches"));

void FComputeGraphTaskWorker::Enqueue(FName InOwnerName, FComputeGraphRenderProxy const* InGraphRenderProxy, TArray<FComputeDataProviderRenderProxy*> InDataProviderRenderProxies)
{
	FGraphInvocation& GraphInvocation = GraphInvocations.AddDefaulted_GetRef();
	GraphInvocation.OwnerName = InOwnerName;
	GraphInvocation.GraphRenderProxy = InGraphRenderProxy;
	GraphInvocation.DataProviderRenderProxies = MoveTemp(InDataProviderRenderProxies);
}

void FComputeGraphTaskWorker::SubmitWork(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	if (GraphInvocations.IsEmpty())
	{
		return;
	}

	{
		SCOPED_DRAW_EVENTF(RHICmdList, ComputeFramework_ExecuteBatches, TEXT("ComputeFramework::ExecuteBatches"));
		SCOPED_GPU_STAT(RHICmdList, ComputeFramework_ExecuteBatches);

		FRDGBuilder GraphBuilder(RHICmdList);

		for (int32 GraphIndex = 0; GraphIndex < GraphInvocations.Num(); ++GraphIndex)
		{
			FGraphInvocation const& GraphInvocation = GraphInvocations[GraphIndex];
			FComputeGraphRenderProxy const* GraphRenderProxy = GraphInvocation.GraphRenderProxy;

			RDG_EVENT_SCOPE(GraphBuilder, "%s:%s", *GraphInvocation.OwnerName.ToString(), *GraphRenderProxy->GraphName.ToString());

			// Do resource allocation for all the data providers in the graph.
			for (int32 DataProviderIndex = 0; DataProviderIndex < GraphInvocation.DataProviderRenderProxies.Num(); ++DataProviderIndex)
			{
				FComputeDataProviderRenderProxy* DataProvider = GraphInvocation.DataProviderRenderProxies[DataProviderIndex];
				if (DataProvider != nullptr)
				{
					DataProvider->AllocateResources(GraphBuilder);
				}
			}

			// Iterate the graph kernels to collect shader bindings and dispatch work.
			for (int32 KernelIndex = 0; KernelIndex < GraphRenderProxy->KernelInvocations.Num(); ++KernelIndex)
			{
				FComputeGraphRenderProxy::FKernelInvocation const& KernelInvocation = GraphRenderProxy->KernelInvocations[KernelIndex];

				RDG_EVENT_SCOPE(GraphBuilder, "%s", *KernelInvocation.KernelName);

				TArray<FIntVector> ThreadCounts;
				const int32 NumSubInvocations = GraphInvocation.DataProviderRenderProxies[KernelInvocation.ExecutionProviderIndex]->GetDispatchThreadCount(ThreadCounts);

				TStridedView<FComputeKernelShader::FParameters> ParameterArray = GraphBuilder.AllocParameters<FComputeKernelShader::FParameters>(KernelInvocation.ShaderParameterMetadata, NumSubInvocations);

				// Iterate shader parameter members to fill the dispatch data structures.
				// We assume that the members were filled out with a single data interface per member, and that the
				// order is the same one defined in the KernelInvocation.BoundProviderIndices.
				TArray<FShaderParametersMetadata::FMember> const& ParamMembers = KernelInvocation.ShaderParameterMetadata->GetMembers();

				FComputeDataProviderRenderProxy::FCollectedDispatchData DispatchData;
				DispatchData.ParameterBuffer = reinterpret_cast<uint8*>(&ParameterArray[0]);
				DispatchData.PermutationId.AddZeroed(NumSubInvocations);

				FComputeDataProviderRenderProxy::FDispatchSetup DispatchSetup{ NumSubInvocations, 0, ParameterArray.GetStride(), 0, GraphRenderProxy->ShaderPermutationVectors[KernelIndex]};

				for (int32 MemberIndex = 0; MemberIndex < ParamMembers.Num(); ++MemberIndex)
				{
					FShaderParametersMetadata::FMember const& Member = ParamMembers[MemberIndex];
					if (ensure(Member.GetBaseType() == EUniformBufferBaseType::UBMT_NESTED_STRUCT))
					{
						const int32 DataProviderIndex = KernelInvocation.BoundProviderIndices[MemberIndex];
						FComputeDataProviderRenderProxy* DataProvider = GraphInvocation.DataProviderRenderProxies[DataProviderIndex];
						if (ensure(DataProvider != nullptr))
						{
							DispatchSetup.ParameterBufferOffset = Member.GetOffset();
							DispatchSetup.ParameterStructSizeForValidation = Member.GetStructMetadata()->GetSize();

							DataProvider->GatherDispatchData(DispatchSetup, DispatchData);
						}
					}
				}

				// Dispatch work to the render graph.
				for (int32 SubInvocationIndex = 0; SubInvocationIndex < NumSubInvocations; ++SubInvocationIndex)
				{
					TShaderRef<FComputeKernelShader> Shader = KernelInvocation.KernelResource->GetShader(DispatchData.PermutationId[SubInvocationIndex]);
					const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCounts[SubInvocationIndex], KernelInvocation.KernelGroupSize);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						{},
						ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
						Shader,
						KernelInvocation.ShaderParameterMetadata,
						&ParameterArray[SubInvocationIndex],
						GroupCount
					);
				}
			}
		}

		// Release any graph resources at the end of graph execution.
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("Release Data Providers"), 
			ERDGPassFlags::None, 
			[this](FRHICommandList&) 
		{
			GraphInvocations.Reset(); 
		});

		// Execute graph.
		// todo[CF]: We can pull this out into calling code so that graph can be shared with other work.
		GraphBuilder.Execute();
	}
}

FComputeGraphTaskWorker::FGraphInvocation::~FGraphInvocation()
{
	// DataProviderRenderProxy objects are created per frame and destroyed here after render work has been submitted.
	// todo[CF]: Some proxies can probably persist, but will need logic to define that and flag when they need recreating.
	for (FComputeDataProviderRenderProxy* DataProvider : DataProviderRenderProxies)
	{
		delete DataProvider;
	}
}
