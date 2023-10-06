// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/DisplayClusterTickableGameObject.h"

void FDisplayClusterTickableGameObject::Tick(float DeltaTime)
{
	if (TickEvent.IsBound())
	{
		TickEvent.Broadcast(DeltaTime);
	}
}
