// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterStageActorComponent.h"
#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

void UDisplayClusterStageActorComponent::SetRootActor(ADisplayClusterRootActor* InRootActor)
{
	RootActor = InRootActor;
}

const TSoftObjectPtr<ADisplayClusterRootActor>& UDisplayClusterStageActorComponent::GetRootActor() const
{
	return RootActor;
}