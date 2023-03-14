// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSpawnable.h"
#include "Misc/LevelSequenceEditorSpawnRegister.h"

/** Movie scene spawn register that knows how to handle spawning objects (actors) for a template sequence  */
class FTemplateSequenceEditorSpawnRegister : public FLevelSequenceEditorSpawnRegister
{
#if WITH_EDITOR
	virtual bool CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const override;
#endif
};
