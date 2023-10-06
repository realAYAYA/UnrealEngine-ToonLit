// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationMapCheckErrorHandler.h"
#include "Logging/MessageLog.h"

void FStreamingGenerationMapCheckErrorHandler::HandleTokenizedMessage(TSharedRef<FTokenizedMessage>&& ErrorMessage)
{
	FMessageLog("MapCheck").AddMessage(ErrorMessage);
}

#endif
