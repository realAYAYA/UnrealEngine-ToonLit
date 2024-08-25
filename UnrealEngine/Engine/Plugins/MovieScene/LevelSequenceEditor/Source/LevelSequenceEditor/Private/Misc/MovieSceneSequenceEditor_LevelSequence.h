// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSequenceEditor.h"
#include "LevelSequenceDirector.h"
#include "Kismet2/KismetEditorUtilities.h"

struct FMovieSceneSequenceEditor_LevelSequence : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		ULevelSequence* LevelSequence = CastChecked<ULevelSequence>(InSequence);
		return LevelSequence->GetDirectorBlueprint();
	}

	virtual UBlueprint* CreateBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
#if WITH_EDITOR
		if (!UMovieScene::IsTrackClassAllowed(ULevelSequenceDirector::StaticClass()))
		{
			return nullptr;
		}
#endif

		UBlueprint* Blueprint = GetBlueprintForSequence(InSequence);
		if (!ensureMsgf(!Blueprint, TEXT("Should not call CreateBlueprintForSequence when one already exists")))
		{
			return Blueprint;
		}

		ULevelSequence* LevelSequence = CastChecked<ULevelSequence>(InSequence);

		Blueprint = FKismetEditorUtilities::CreateBlueprint(ULevelSequenceDirector::StaticClass(), InSequence, FName(*LevelSequence->GetDirectorBlueprintName()), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		Blueprint->ClearFlags(RF_Standalone);

		LevelSequence->SetDirectorBlueprint(Blueprint);
		return Blueprint;
	}
};
