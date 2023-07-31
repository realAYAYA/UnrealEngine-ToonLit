// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchActorFactory.h"
#include "GameFramework/Actor.h"
#include "SwitchActor.h"

USwitchActorFactory::USwitchActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("SwitchActorFactory", "SwitchActorDisplayName", "Switch Actor");
	NewActorClass = ASwitchActor::StaticClass();
}