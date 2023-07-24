// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/RestorationEvents/RemoveComponentScope.h"

#include "Components/ActorComponent.h"
#include "LevelSnapshotsModule.h"
#include "Components/ActorComponent.h"

UE::LevelSnapshots::Private::FRemoveComponentScope::FRemoveComponentScope(UActorComponent* RemovedComponent)
	:
	Params({ RemovedComponent->GetOwner(), RemovedComponent->GetFName(), RemovedComponent })
{
	FLevelSnapshotsModule::GetInternalModuleInstance().OnPreRemoveComponent(RemovedComponent);
}

UE::LevelSnapshots::Private::FRemoveComponentScope::~FRemoveComponentScope()
{
	FLevelSnapshotsModule::GetInternalModuleInstance().OnPostRemoveComponent(Params);
}
