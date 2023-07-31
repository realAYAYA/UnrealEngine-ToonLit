// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationMapCheckErrorHandler.h"
#include "GameFramework/Actor.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "GameFramework/Actor.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"

#define LOCTEXT_NAMESPACE "WorldPartition"

void FStreamingGenerationMapCheckErrorHandler::AddAdditionalNameToken(TSharedRef<FTokenizedMessage>& InMessage, const FName& InErrorName)
{
	InMessage->AddToken(FMapErrorToken::Create(InErrorName));
}

void FStreamingGenerationMapCheckErrorHandler::HandleTokenizedMessage(TSharedRef<FTokenizedMessage>&& ErrorMessage)
{
	FMessageLog("MapCheck").AddMessage(ErrorMessage);
}

#undef LOCTEXT_NAMESPACE

#endif
