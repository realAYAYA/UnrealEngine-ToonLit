// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
const FWorldPartitionActorDescView& IStreamingGenerationContext::FActorInstance::GetActorDescView() const
{
	return ActorSetInstance->ContainerInstance->ActorDescViewMap->FindByGuidChecked(ActorGuid);
}

const FActorContainerID& IStreamingGenerationContext::FActorInstance::GetContainerID() const
{
	return ActorSetInstance->ContainerID;
}

const FTransform& IStreamingGenerationContext::FActorInstance::GetTransform() const
{
	return ActorSetInstance->Transform;
}

const UActorDescContainer* IStreamingGenerationContext::FActorInstance::GetActorDescContainer() const
{
	return ActorSetInstance->ContainerInstance->ActorDescContainer;
}
#endif