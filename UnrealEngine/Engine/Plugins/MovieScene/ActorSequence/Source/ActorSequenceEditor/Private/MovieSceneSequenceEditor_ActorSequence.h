// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceEditor.h"
#include "ActorSequence.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"

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
