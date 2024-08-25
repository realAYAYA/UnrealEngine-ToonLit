// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorToolBase.h"
#include "GameFramework/Actor.h"

bool UAvaInteractiveToolsActorToolBase::OnBegin()
{
	if (ActorClass == nullptr)
	{
		return false;
	}

	return Super::OnBegin();
}

void UAvaInteractiveToolsActorToolBase::DefaultAction()
{
	SpawnedActor = SpawnActor(ActorClass, false);

	Super::DefaultAction();
}
