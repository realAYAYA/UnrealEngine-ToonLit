// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsEngine/ClusterUnionActor.h"

#include "PhysicsEngine/ClusterUnionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClusterUnionActor)

AClusterUnionActor::AClusterUnionActor(const FObjectInitializer& ObjectInitializer)
	: AActor(ObjectInitializer)
{
	ClusterUnion = CreateDefaultSubobject<UClusterUnionComponent>(TEXT("ClusterUnion"));
	SetRootComponent(ClusterUnion);

	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	SetReplicatingMovement(true);
}