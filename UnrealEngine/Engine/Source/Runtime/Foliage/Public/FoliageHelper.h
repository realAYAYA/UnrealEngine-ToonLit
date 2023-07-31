// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Foliage Helper class
 */
class FFoliageHelper
{
public:
#if WITH_EDITOR
	static void SetIsOwnedByFoliage(AActor* InActor, bool bOwned = true) 
	{ 
		static FName NAME_FoliageActorInstanceTag(TEXT("FoliageActorInstance"));
		if (InActor) 
		{ 
			FSetActorHiddenInSceneOutliner SetHidden(InActor, bOwned);
			if (bOwned)
			{
				InActor->Tags.AddUnique(NAME_FoliageActorInstanceTag);
			}
			else
			{
				InActor->Tags.Remove(NAME_FoliageActorInstanceTag);
			}
		} 
	}
	static bool IsOwnedByFoliage(const AActor* InActor) { return InActor != nullptr && InActor->ActorHasTag(TEXT("FoliageActorInstance")); }
#endif
};