// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencer.h"

class MOVIESCENETOOLS_API FLevelSequenceFBXInterop
{
public:

	FLevelSequenceFBXInterop(TSharedPtr<ISequencer> InSequencer);

	/** Imports the animation from an fbx file. */
	void ImportFBX();
	void ImportFBXOntoSelectedNodes();

	/** Exports the animation to an fbx file. */
	void ExportFBX();

private:

	/** Exports sequence to a FBX file */
	void ExportFBXInternal(const FString& ExportFilename, const TArray<FGuid>& Bindings, const TArray<UMovieSceneTrack*>& Tracks);

private:

	TSharedPtr<ISequencer> Sequencer;
};

