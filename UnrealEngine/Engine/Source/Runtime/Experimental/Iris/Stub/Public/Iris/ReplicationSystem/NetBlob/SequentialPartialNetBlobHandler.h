// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Iris/ReplicationSystem/NetBlob/NetBlobHandler.h"
#include "SequentialPartialNetBlobHandler.generated.h"

UCLASS(transient, MinimalApi)
class USequentialPartialNetBlobHandlerConfig : public UObject
{
	GENERATED_BODY()
};

UCLASS(transient, MinimalApi)
class USequentialPartialNetBlobHandler : public UNetBlobHandler
{
	GENERATED_BODY()
};
