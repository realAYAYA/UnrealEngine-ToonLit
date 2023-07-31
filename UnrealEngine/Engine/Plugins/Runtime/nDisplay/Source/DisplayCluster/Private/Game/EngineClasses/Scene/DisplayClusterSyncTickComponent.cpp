// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterSyncTickComponent.h"

#include "IPDisplayCluster.h"
#include "Misc/DisplayClusterGlobals.h"


UDisplayClusterSyncTickComponent::UDisplayClusterSyncTickComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}

void UDisplayClusterSyncTickComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (GDisplayCluster->GetOperationMode() != EDisplayClusterOperationMode::Disabled)
	{
		GDisplayCluster->Tick(DeltaTime);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
