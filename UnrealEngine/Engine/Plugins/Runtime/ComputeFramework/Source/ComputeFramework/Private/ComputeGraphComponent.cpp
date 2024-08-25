// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeGraphComponent.h"
#include "ComputeFramework/ComputeFramework.h"
#include "ComputeFramework/ComputeGraph.h"
#include "ComputeWorkerInterface.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ComputeGraphComponent)

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
}

void UComputeGraphComponent::DestroyDataProviders()
{
	ComputeGraphInstance.DestroyDataProviders();
}

void UComputeGraphComponent::QueueExecute()
{
	if (ComputeGraph != nullptr)
	{
		MarkRenderDynamicDataDirty();
	}
}

void UComputeGraphComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	QueueExecute();
}

void UComputeGraphComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

	ComputeGraphInstance.EnqueueWork(ComputeGraph, GetScene(), ComputeTaskExecutionGroup::EndOfFrameUpdate, GetOwner()->GetFName(), FSimpleDelegate(), this);
}

void UComputeGraphComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	
	ComputeFramework::AbortWork(GetScene(), this);
}