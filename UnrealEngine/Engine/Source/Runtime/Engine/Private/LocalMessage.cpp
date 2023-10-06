// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/LocalMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalMessage)

FClientReceiveData::FClientReceiveData()
	: LocalPC(NULL)
	, MessageType(FName(TEXT("None")))
	, MessageIndex(-1)
	, MessageString(TEXT(""))
	, RelatedPlayerState_1(NULL)
	, RelatedPlayerState_2(NULL)
	, OptionalObject(NULL)
{
	
}

ULocalMessage::ULocalMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

