// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "UObject/SoftObjectPath.h"

// Helpers
FString IStreamingGenerationErrorHandler::GetFullActorName(const FWorldPartitionActorDescView& ActorDescView)
{
	const FSoftObjectPath ActorPath = ActorDescView.GetActorSoftPath();
	return ActorDescView.GetActorLabelOrName().ToString() + TEXT(" (") + ActorPath.GetLongPackageName() + TEXT(")");
}
#endif
