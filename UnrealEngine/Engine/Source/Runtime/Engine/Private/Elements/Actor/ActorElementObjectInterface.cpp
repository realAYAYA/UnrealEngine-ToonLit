// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementObjectInterface.h"
#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorElementObjectInterface)

UObject* UActorElementObjectInterface::GetObject(const FTypedElementHandle& InElementHandle)
{
	return ActorElementDataUtil::GetActorFromHandle(InElementHandle);
}

