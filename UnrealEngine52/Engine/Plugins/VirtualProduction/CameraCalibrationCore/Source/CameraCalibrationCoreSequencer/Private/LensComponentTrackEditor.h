// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "MovieSceneLensComponentTrack.h"

/** A property track editor for Lens Component */
class FLensComponentTrackEditor : public FKeyframeTrackEditor<UMovieSceneLensComponentTrack>
{
public:

	FLensComponentTrackEditor(TSharedRef<ISequencer> InSequencer) : FKeyframeTrackEditor<UMovieSceneLensComponentTrack>(InSequencer)
	{
	}

	/**
	* Creates an instance of this class.  Called by a sequencer
	*
	* @param OwningSequencer The sequencer instance to be used by this tool
	* @return The new instance of this class
	*/
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:
	//~ Begin ISequencerTrackEditor interface
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	//~ End ISequencerTrackEditor interface
};
