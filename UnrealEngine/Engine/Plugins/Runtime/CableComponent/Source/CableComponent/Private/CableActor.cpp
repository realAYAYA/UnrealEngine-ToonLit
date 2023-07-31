// Copyright Epic Games, Inc. All Rights Reserved.

#include "CableActor.h"
#include "CableComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CableActor)


ACableActor::ACableActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CableComponent = CreateDefaultSubobject<UCableComponent>(TEXT("CableComponent0"));
	RootComponent = CableComponent;
}

