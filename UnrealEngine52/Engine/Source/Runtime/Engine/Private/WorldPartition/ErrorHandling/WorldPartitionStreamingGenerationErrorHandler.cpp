// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

FString IStreamingGenerationErrorHandler::GetActorName(const FWorldPartitionActorDescView& ActorDescView)
{
	const FString ActorPath = ActorDescView.GetActorSoftPath().GetLongPackageName();
	const FString ActorLabel = ActorDescView.GetActorLabelOrName().ToString();
	return ActorPath + TEXT(".") + ActorLabel;
}

FString IStreamingGenerationErrorHandler::GetFullActorName(const FWorldPartitionActorDescView& ActorDescView)
{
	const FString ActorPackage = ActorDescView.GetActorPackage().ToString();
	return GetActorName(ActorDescView) + TEXT(" (") + ActorPackage + TEXT(")");
}
#endif
