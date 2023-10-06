// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "MovieSceneSequenceEditor.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"

struct FMovieSceneSequenceEditor_WidgetAnimation : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		return InSequence->GetTypedOuter<UBlueprint>();
	}
};