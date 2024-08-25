// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"

class AActor;

/**
 * Pushes a new context initialized from the provided actor.
 */
class ENGINE_API FScopedActorEditorContextFromActor
{
public:
	FScopedActorEditorContextFromActor(AActor* InActor);
	~FScopedActorEditorContextFromActor();
};

#endif