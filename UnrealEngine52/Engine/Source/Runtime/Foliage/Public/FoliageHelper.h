// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * Foliage Helper class
 */
class FFoliageHelper
{
public:
#if WITH_EDITOR
	static inline FName GetFoliageActorInstanceTag()
	{
		static FName NAME_FoliageActorInstanceTag(TEXT("FoliageActorInstance"));
		return NAME_FoliageActorInstanceTag;
	}

	static inline void SetIsOwnedByFoliage(AActor* InActor, bool bOwned = true) 
	{ 
		if (InActor) 
		{ 
			FSetActorHiddenInSceneOutliner SetHidden(InActor, bOwned);
			if (bOwned)
			{
				InActor->Tags.AddUnique(GetFoliageActorInstanceTag());
			}
			else
			{
				InActor->Tags.Remove(GetFoliageActorInstanceTag());
			}
		} 
	}

	static inline bool IsOwnedByFoliage(const AActor* InActor)
	{
		return InActor && InActor->ActorHasTag(GetFoliageActorInstanceTag());
	}
#endif
};