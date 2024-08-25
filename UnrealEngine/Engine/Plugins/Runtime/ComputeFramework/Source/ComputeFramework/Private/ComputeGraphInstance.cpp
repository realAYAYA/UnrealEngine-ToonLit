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

#include UE_INLINE_GENERATED_CPP_BY_NAME(ComputeGraphInstance)

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

bool FComputeGraphInstance::EnqueueWork(UComputeGraph* InComputeGraph, FSceneInterface const* InScene, FName InExecutionGroupName, FName InOwnerName, FSimpleDelegate InFallbackDelegate, UObject* InOwnerPointer)
{
	if (InComputeGraph == nullptr || InScene == nullptr)
	{
		return false;
	}

	if (!ComputeFramework::IsEnabled() || FComputeFrameworkModule::GetComputeSystem() == nullptr)
	{
		return false;
	}

	// Don't submit work if we don't have all of the expected bindings.
	if (!InComputeGraph->ValidateProviders(DataProviders))
	{
		return false;
	}

	// Lookup the compute worker associated with this scene.
	FComputeGraphTaskWorker* ComputeGraphWorker = FComputeFrameworkModule::GetComputeSystem()->GetComputeWorker(InScene);
	if (!ensure(ComputeGraphWorker))
	{
		return false;
	}

	FComputeGraphRenderProxy const* GraphRenderProxy = InComputeGraph->GetRenderProxy();
	if (GraphRenderProxy == nullptr)
	{
		// This can happen if we have deferred compilation.
		// Trigger compilation now.
		ensure(ComputeFramework::IsDeferredCompilation());
		InComputeGraph->UpdateResources();
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
		[ComputeGraphWorker, InExecutionGroupName, InOwnerName, SortPriority = GraphSortPriority, GraphRenderProxy, MovedDataProviderRenderProxies = MoveTemp(DataProviderRenderProxies), InFallbackDelegate, InOwnerPointer](FRHICommandListImmediate& RHICmdList)
		{
			// Compute graph scheduler will take ownership of the provider proxies.
			ComputeGraphWorker->Enqueue(InExecutionGroupName, InOwnerName, SortPriority, GraphRenderProxy, MovedDataProviderRenderProxies, InFallbackDelegate, InOwnerPointer);
		});

	return true;
}
