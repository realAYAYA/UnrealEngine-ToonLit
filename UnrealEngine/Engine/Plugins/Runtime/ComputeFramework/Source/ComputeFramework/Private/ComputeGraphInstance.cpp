// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeGraphInstance.h"

#include "ComputeFramework/ComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeFramework/ComputeFramework.h"
#include "ComputeFramework/ComputeFrameworkModule.h"
#include "ComputeFramework/ComputeGraph.h"
#include "ComputeFramework/ComputeGraphRenderProxy.h"
#include "ComputeFramework/ComputeGraphWorker.h"
#include "ComputeFramework/ComputeSystem.h"

void FComputeGraphInstance::CreateDataProviders(UComputeGraph* InComputeGraph, int32 InBindingIndex, TObjectPtr<UObject> InBindingObject)
{
	if (InComputeGraph != nullptr)
	{
		InComputeGraph->CreateDataProviders(InBindingIndex, InBindingObject, DataProviders);
	}
}

void FComputeGraphInstance::DestroyDataProviders()
{
	DataProviders.Reset();
}

bool FComputeGraphInstance::ValidateDataProviders(UComputeGraph* InComputeGraph) const
{
	return InComputeGraph != nullptr && InComputeGraph->IsCompiled() && InComputeGraph->ValidateGraph() && InComputeGraph->ValidateProviders(DataProviders);
}

bool FComputeGraphInstance::EnqueueWork(UComputeGraph* InComputeGraph, FSceneInterface const* InScene, FName InOwnerName)
{
	if (InComputeGraph == nullptr || InScene == nullptr)
	{
		// todo[CF]: We should have a default fallback for all cases where we can't submit work.
		return false;
	}

	if (!ComputeFramework::IsEnabled() || FComputeFrameworkModule::GetComputeSystem() == nullptr)
	{
		return false;
	}

	// Lookup the compute worker associated with this scene.
	FComputeGraphTaskWorker* ComputeGraphWorker = FComputeFrameworkModule::GetComputeSystem()->GetComputeWorker(InScene);
	if (!ensure(ComputeGraphWorker))
	{
		return false;
	}

	// Don't submit work if we don't have all of the expected bindings.
	// If we hit the ensure then something invalidated providers without calling CreateDataProviders().
	// Those paths DO need fixing. We can remove the ensure() if we ever feel safe enough!
	const bool bValidProviders = InComputeGraph->ValidateProviders(DataProviders);
	if (!ensure(bValidProviders))
	{
		return false;
	}

	FComputeGraphRenderProxy const* GraphRenderProxy = InComputeGraph->GetRenderProxy();
	if (!ensure(GraphRenderProxy))
	{
		return false;
	}

	TArray<FComputeDataProviderRenderProxy*> DataProviderRenderProxies;
	for (UComputeDataProvider* DataProvider : DataProviders)
	{
		// Be sure to add null provider slots because we want to maintain consistent array indices.
		// Note that we expect GetRenderProxy() to return a pointer that we can own and call delete on.
		FComputeDataProviderRenderProxy* ProviderProxy = DataProvider != nullptr ? DataProvider->GetRenderProxy() : nullptr;
		DataProviderRenderProxies.Add(ProviderProxy);
	}

	ENQUEUE_RENDER_COMMAND(ComputeFrameworkEnqueueExecutionCommand)(
		[ComputeGraphWorker, InOwnerName, GraphRenderProxy, MovedDataProviderRenderProxies = MoveTemp(DataProviderRenderProxies)](FRHICommandListImmediate& RHICmdList)
		{
			// Compute graph scheduler will take ownership of the provider proxies.
			ComputeGraphWorker->Enqueue(InOwnerName, GraphRenderProxy, MovedDataProviderRenderProxies);
		});

	return true;
}
