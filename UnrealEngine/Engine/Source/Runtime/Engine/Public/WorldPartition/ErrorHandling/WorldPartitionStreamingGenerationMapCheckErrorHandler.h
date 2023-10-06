// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationTokenizedMessageErrorHandler.h"

class FStreamingGenerationMapCheckErrorHandler : public ITokenizedMessageErrorHandler
{
protected:
	ENGINE_API virtual void HandleTokenizedMessage(TSharedRef<FTokenizedMessage>&& ErrorMessage) override;
};
#endif
