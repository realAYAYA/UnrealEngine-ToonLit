// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeGraphComponent.h"

#include "ComputeFramework/ComputeGraph.h"
#include "GameFramework/Actor.h"

UComputeGraphComponent::UComputeGraphComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// By default don't tick and allow any queuing of work to be handled by blueprint.
	// Ticking can be turned on by some systems that need it (such as editor window).
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UComputeGraphComponent::~UComputeGraphComponent()
{
}

void UComputeGraphComponent::CreateDataProviders(int32 InBindingIndex, UObject* InBindingObject)
{
 	ComputeGraphInstance.CreateDataProviders(ComputeGraph, InBindingIndex, InBindingObject);

	// We only want to queue work after validating the new providers.
	bValidProviders = false;
}

void UComputeGraphComponent::DestroyDataProviders()
{
	ComputeGraphInstance.DestroyDataProviders();
	bValidProviders = false;
}

void UComputeGraphComponent::QueueExecute()
{
	if (ComputeGraph == nullptr)
	{
		// todo[CF]: We should have a default fallback for all cases where we can't submit work.
		return;
	}

	// Don't submit work if we don't have all of the expected bindings.
	bValidProviders = bValidProviders || ComputeGraphInstance.ValidateDataProviders(ComputeGraph);
	if (!bValidProviders)
	{
		return;
	}

	MarkRenderDynamicDataDirty();
}

void UComputeGraphComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	QueueExecute();
}

void UComputeGraphComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	
	ComputeGraphInstance.EnqueueWork(ComputeGraph, GetScene(), GetOwner()->GetFName());
}
