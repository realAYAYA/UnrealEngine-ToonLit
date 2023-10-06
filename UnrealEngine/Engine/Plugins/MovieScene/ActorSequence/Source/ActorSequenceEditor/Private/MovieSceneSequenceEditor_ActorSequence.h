// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorSequenceComponent.h"
#include "GameFramework/Actor.h"
#include "MovieSceneSequenceEditor.h"
#include "ActorSequence.h"
#include "Engine/LevelScriptBlueprint.h"

struct FMovieSceneSequenceEditor_ActorSequence : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		UActorSequence* ActorSequence = CastChecked<UActorSequence>(InSequence);
		if (UBlueprint* Blueprint = ActorSequence->GetParentBlueprint())
		{
			return Blueprint;
		}

		UActorSequenceComponent* Component = ActorSequence->GetTypedOuter<UActorSequenceComponent>();
		ULevel* Level = (Component && Component->GetOwner()) ? Component->GetOwner()->GetLevel() : nullptr;

		bool bDontCreateNewBlueprint = true;
		return Level ? Level->GetLevelScriptBlueprint(bDontCreateNewBlueprint) : nullptr;
	}

	virtual UBlueprint* CreateBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		UActorSequence* ActorSequence = CastChecked<UActorSequence>(InSequence);
		check(!ActorSequence->GetParentBlueprint());

		UActorSequenceComponent* Component = ActorSequence->GetTypedOuter<UActorSequenceComponent>();
		ULevel* Level = (Component && Component->GetOwner()) ? Component->GetOwner()->GetLevel() : nullptr;

		bool bDontCreateNewBlueprint = false;
		return Level ? Level->GetLevelScriptBlueprint(bDontCreateNewBlueprint) : nullptr;
	}
};
