// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageContext.h"
#include "QuicMessagingTestTypes.generated.h"


USTRUCT()
struct FQuicMockMessage
{

	GENERATED_BODY()

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	TArray<uint8> Data;

	FQuicMockMessage()
	{
		Data.AddUninitialized(64);
	}

	FQuicMockMessage(int32 DataSize)
	{
		Data.AddUninitialized(DataSize);
	}

};
