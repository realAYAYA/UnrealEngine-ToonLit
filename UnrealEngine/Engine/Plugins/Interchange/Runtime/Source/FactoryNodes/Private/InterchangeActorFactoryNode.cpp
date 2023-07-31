// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeActorFactoryNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeActorFactoryNode)

#if WITH_ENGINE
	#include "GameFramework/Actor.h"
#endif

UClass* UInterchangeActorFactoryNode::GetObjectClass() const
{
#if WITH_ENGINE
	FString ActorClassName;
	if (GetCustomActorClassName(ActorClassName))
	{
		UClass* ActorClass = FindObject<UClass>(nullptr, *ActorClassName);
		if (ActorClass->IsChildOf<AActor>())
		{
			return ActorClass;
		}
	}

	return AActor::StaticClass();
#else
	return nullptr;
#endif
}
