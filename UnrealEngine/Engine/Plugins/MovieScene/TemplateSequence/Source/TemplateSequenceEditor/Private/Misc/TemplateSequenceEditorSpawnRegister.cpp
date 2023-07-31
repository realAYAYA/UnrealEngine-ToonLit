// Copyright Epic Games, Inc. All Rights Reserved.

#include "TemplateSequenceEditorSpawnRegister.h"

#if WITH_EDITOR

bool FTemplateSequenceEditorSpawnRegister::CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const
{
	return false;
}

#endif