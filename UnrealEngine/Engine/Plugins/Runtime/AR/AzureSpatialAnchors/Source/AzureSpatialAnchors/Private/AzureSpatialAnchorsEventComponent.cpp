// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsEventComponent.h"
#include "Misc/CoreDelegates.h"

UAzureSpatialAnchorsEventComponent::UAzureSpatialAnchorsEventComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAzureSpatialAnchorsEventComponent::OnRegister()
{
	Super::OnRegister();

	IAzureSpatialAnchors::ASAAnchorLocatedDelegate.AddUObject(this, &UAzureSpatialAnchorsEventComponent::ASAAnchorLocatedDelegate_Handler);
	IAzureSpatialAnchors::ASALocateAnchorsCompletedDelegate.AddUObject(this, &UAzureSpatialAnchorsEventComponent::ASALocateAnchorsCompleteDelegate_Handler);
	IAzureSpatialAnchors::ASASessionUpdatedDelegate.AddUObject(this, &UAzureSpatialAnchorsEventComponent::ASASessionUpdatedDelegate_Handler);
}

void UAzureSpatialAnchorsEventComponent::OnUnregister()
{
	Super::OnUnregister();

	IAzureSpatialAnchors::ASAAnchorLocatedDelegate.RemoveAll(this);
	IAzureSpatialAnchors::ASALocateAnchorsCompletedDelegate.RemoveAll(this);
	IAzureSpatialAnchors::ASASessionUpdatedDelegate.RemoveAll(this);
}
